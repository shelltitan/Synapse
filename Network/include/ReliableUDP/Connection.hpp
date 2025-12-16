#pragma once
#include <bitset>
#include <cmath>
#include <span>

#include <Lock.h>
#include <Memory.h>
#include <ReadStream.h>

#include "ReliableBuffer.h"
#include "SerialiseReliable.h"

namespace CoreReliableUDP {
    enum CounterTypes {
        PacketsSent,
        PacketsReceived,
        PacketsAcknowledged,
        StalePackets,
        InvalidPackets,
        OversizedSendPackets,
        OversizedReceivePackets,
        FragmentedPacketsSent,
        FragmantedPacketsReceived,
        InvalidFragmantedPackets,
        MAX
    };

    inline constexpr int MIN_RUDP_HEADER_BYTES = 4;
    inline constexpr int MAX_RUDP_HEADER_BYTES = 9;
    inline constexpr int FRAGMENT_HEADER_BYTES = 5;

    struct ReceivedPacketData {
        std::uint64_t time;
        std::uint32_t packet_bytes;
    };

    template <int max_number_of_fragments>
    struct FragmentReassemblyData {
        std::uint16_t sequence;
        std::uint16_t ack;
        std::uint32_t ack_bits;
        int num_fragments_received;
        int num_fragments_total;
        std::byte* packet_data;
        int packet_bytes;
        int packet_header_bytes;
        std::array<std::uint8_t, max_number_of_fragments> fragment_received;
    };

    struct SentPacketData {
        std::uint64_t time;
        std::uint32_t acked : 1;
        std::uint32_t packet_bytes : 31;
    };

    // note: UDP over IPv4 = 20 + 8 bytes, UDP over IPv6 = 40 + 8 bytes
    inline constexpr std::uint32_t IP_HEADER_SIZE_IPV4{ 28 };
    inline constexpr std::uint32_t IP_HEADER_SIZE_IPV6{ 48 };

    // statistics related constants
    inline constexpr float ROUND_TRIP_TIME_SMOOTHING_FACTOR{ 0.0025f };
    inline constexpr float PACKET_LOSS_SMOOTHING_FACTOR{ 0.1f };
    inline constexpr float BANDWIDTH_SMOOTHING_FACTOR{ 0.1f };
    inline constexpr int ROUND_TRIP_TIME_HISTORY_SIZE{ 512 };

    /// \todo locking
    /*
     * max_packet_size the largest packet made up by multiple fragments (FRAGMENT_SIZE * max_number_of_fragments)
     * class_connection_manager has to implement
     *
     * Send_rUDPPacket(int connection_index, std::byte* data_to_be_sent, int buffer_size)
     * Send_rUDPPacket is expected to copy the content of the given buffer, and handle it from that point
     *
     * bool ProcessDeserialised_rUDPPacket(int connection_index, std::uint16_t packet_sequence, std::byte* packet_data, int packet_bytes)
     * ProcessDeserialised_rUDPPacket is expected to return a bool whether the packet has been accepted or not, the packet_data gets deallocated
     * at the end of the call if it is a fragmented packet, in a non-fragmanted case it depends on how the caller of Deserialise_rUDPPacket manages the buffer
     *
     * std::uint64 GetTime()
     * Expects milliseconds to be returned
     *
     * bool IsConnectionConnected(int connection_index)
     * Expects a bool to decide if the connection is connected
     */
    template <class class_connection_manager, unsigned int max_connection_count, unsigned int max_packet_size, unsigned int fragment_above,
        unsigned int max_number_of_fragments, unsigned int fragment_size,
        unsigned int acknowledge_buffer_size, unsigned int sent_packet_buffer_size,
        unsigned int received_packets_buffer_size, unsigned int fragment_reassembly_buffer_size>
    class rUDPConnetion {
    public:
        rUDPConnetion(class_connection_manager* connection_manager) {
            static_assert(fragment_above > 0);
            static_assert(max_number_of_fragments > 0);
            static_assert(fragment_size > 0);
            static_assert(acknowledge_buffer_size > 0);
            static_assert(sent_packet_buffer_size > 0);
            static_assert(received_packets_buffer_size > 0);
            static_assert(fragment_reassembly_buffer_size > 0);
            static_assert(max_connection_count > 0);
            m_connection_manager = connection_manager;
            ResetAll();
        }
        ~rUDPConnetion() {
            ResetAll();
        }

        void ResetAll() {
            for (int i = 0; i < (fragment_reassembly_buffer_size * max_connection_count); ++i) {
                FragmentReassemblyData<max_number_of_fragments>* reassembly_data = m_fragment_reassembly.GetAtIndex(int(i / fragment_reassembly_buffer_size), i % fragment_reassembly_buffer_size);

                if (reassembly_data && reassembly_data->packet_data) {
                    Global::GMemory->Release(reassembly_data->packet_data);
                    reassembly_data->packet_data = nullptr;
                }
            }

            m_sequence.fill(0);
            m_acknowledgments.fill(0);
            m_number_of_acknowledgements.fill(0);

            m_round_trip_time.fill(0);
            m_round_trip_time_history_buffer.fill(0);
            m_round_trip_time_minimum.fill(0);
            m_round_trip_time_maximum.fill(0);
            m_round_trip_time_average.fill(0);
            m_max_jitter_from_minimum_rtt.fill(0);
            m_average_jitter_from_minimum_rtt.fill(0);
            m_std_jitter_from_average_rtt.fill(0);
            m_packet_loss.fill(0);
            m_sent_bandwidth_kbps.fill(0);
            m_received_bandwidth_kbps.fill(0);
            m_acknowledged_bandwidth_kbps.fill(0);

            m_counters.fill(0);

            m_sent_packets.ResetAll();
            m_received_packets.ResetAll();
            m_fragment_reassembly.ResetAll();
        }

        void Reset(int connection_index) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);

            for (int i = 0; i < fragment_reassembly_buffer_size; ++i) {
                FragmentReassemblyData<max_number_of_fragments>* reassembly_data = m_fragment_reassembly.GetAtIndex(connection_index, i % fragment_reassembly_buffer_size);

                if (reassembly_data && reassembly_data->packet_data) {
                    Global::GMemory->Release(reassembly_data->packet_data);
                    reassembly_data->packet_data = nullptr;
                }
            }

            m_sequence[connection_index] = 0;
            std::fill(&m_acknowledgments[connection_index * acknowledge_buffer_size], &m_acknowledgments[(connection_index + 1) * acknowledge_buffer_size], 0);
            m_number_of_acknowledgements[connection_index] = 0;

            m_round_trip_time[connection_index] = 0.0;
            std::fill(&m_round_trip_time_history_buffer[connection_index * ROUND_TRIP_TIME_HISTORY_SIZE], &m_round_trip_time_history_buffer[(connection_index + 1) * ROUND_TRIP_TIME_HISTORY_SIZE], 0.0f);
            m_round_trip_time_minimum[connection_index] = 0.0;
            m_round_trip_time_maximum[connection_index] = 0.0;
            m_round_trip_time_average[connection_index] = 0.0;
            m_max_jitter_from_minimum_rtt[connection_index] = 0.0;
            m_average_jitter_from_minimum_rtt[connection_index] = 0.0;
            m_std_jitter_from_average_rtt[connection_index] = 0.0;
            m_packet_loss[connection_index] = 0.0;
            m_sent_bandwidth_kbps[connection_index] = 0.0;
            m_received_bandwidth_kbps[connection_index] = 0.0;
            m_acknowledged_bandwidth_kbps[connection_index] = 0.0;

            std::fill(&m_counters[connection_index * CounterTypes::MAX], &m_counters[(connection_index + 1) * CounterTypes::MAX], 0);

            m_sent_packets.Reset(connection_index);
            m_received_packets.Reset(connection_index);
            m_fragment_reassembly.Reset(connection_index);
        }

        template <bool isIPV6>
        void SerialiseTo_rUDPPacket(int connection_index, std::byte* packet_data, int packet_bytes) {
            ASSERT_CRASH(packet_data != nullptr);
            ASSERT_CRASH(packet_bytes > 0);
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);

            if (packet_bytes > max_packet_size) {
                CORE_DEBUG("[rUDPConnetion] Packet too large to send. Packet is {} bytes, maximum is {}, connection index {}", packet_bytes, max_packet_size, connection_index);
                ++m_counters[connection_index * CounterTypes::MAX + CounterTypes::OversizedSendPackets];
                return;
            }

            uint16_t current_sequence = m_sequence[connection_index]++;
            uint16_t acknowledgement;
            uint32_t acknowledgement_bits;

            m_received_packets.GenerateAcknowledgementBits(connection_index, acknowledgement, acknowledgement_bits);

            CORE_DEBUG("[rUDPConnetion] Sending packet sequence {}, connection index {}", current_sequence, connection_index);

            SentPacketData* sent_packet_data = m_sent_packets.Insert(connection_index, current_sequence);

            ASSERT_CRASH(sent_packet_data != nullptr);

            sent_packet_data->time = m_connection_manager->GetTime();
            if constexpr (isIPV6) {
                sent_packet_data->packet_bytes = IP_HEADER_SIZE_IPV6 + packet_bytes;
            }
            else {
                sent_packet_data->packet_bytes = IP_HEADER_SIZE_IPV4 + packet_bytes;
            }

            sent_packet_data->acked = 0;

            if (packet_bytes <= fragment_above) {
                // regular packet

                CORE_DEBUG("[rUDPConnetion] Sending packet sequnce {} without fragmentation, connection index {}", current_sequence, connection_index);

                std::byte* transmit_packet_data = static_cast<std::byte*>(Global::GMemory->Allocate(packet_bytes + MAX_RUDP_HEADER_BYTES));

                int packet_header_bytes = WritePacketHeader(transmit_packet_data, current_sequence, acknowledgement, acknowledgement_bits);

                (void)std::copy_n(packet_data, packet_bytes, transmit_packet_data + packet_header_bytes);

                m_connection_manager->Send_rUDPPacket(connection_index, transmit_packet_data, packet_header_bytes + packet_bytes);

                Global::GMemory->Release(transmit_packet_data);
            }
            else {
                // fragmented packet

                int number_of_fragments = (packet_bytes / fragment_size) + ((packet_bytes % fragment_size) != 0 ? 1 : 0);

                ASSERT_CRASH(number_of_fragments > 0);
                ASSERT_CRASH(number_of_fragments <= max_number_of_fragments);

                CORE_DEBUG("[rUDPConnetion] Sending packet sequnce {} as {} fragments, connection index {}", current_sequence, number_of_fragments, connection_index);

                std::byte* fragment_packet_data = static_cast<std::byte*>(Global::GMemory->Allocate(FRAGMENT_HEADER_BYTES + MAX_RUDP_HEADER_BYTES + fragment_size));

                std::byte* q = packet_data;

                std::byte* end = q + packet_bytes;

                int fragment_id;
                for (fragment_id = 0; fragment_id < number_of_fragments; ++fragment_id) {
                    int fragment_packet_bytes = WriteFaragmentHeader<fragment_size>(fragment_packet_data, &q, end, number_of_fragments, fragment_id, current_sequence, acknowledgement, acknowledgement_bits);
                    m_connection_manager->Send_rUDPPacket(connection_index, fragment_packet_data, fragment_packet_bytes);
                    ++m_counters[connection_index * CounterTypes::MAX + CounterTypes::FragmentedPacketsSent];
                }

                Global::GMemory->Release(fragment_packet_data);
            }

            ++m_counters[connection_index * CounterTypes::MAX + CounterTypes::PacketsSent];
        }

        template <bool isIPV6>
        void Deserialise_rUDPPacket(int connection_index, std::byte* packet_data, int packet_bytes) {
            ASSERT_CRASH(packet_data != nullptr);
            ASSERT_CRASH(packet_bytes > 0);
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);

            if (packet_bytes > max_packet_size + MAX_RUDP_HEADER_BYTES + FRAGMENT_HEADER_BYTES) {
                CORE_DEBUG("[rUDPConnetion] Packet too large to receive. Packet is at least {} bytes, maximum is {}, connection index {}.", packet_bytes - (MAX_RUDP_HEADER_BYTES + FRAGMENT_HEADER_BYTES), max_packet_size, connection_index);
                ++m_counters[connection_index * CounterTypes::MAX + CounterTypes::OversizedReceivePackets];
                return;
            }

            std::uint8_t prefix_byte = static_cast<std::uint8_t>(packet_data[0]);

            if ((prefix_byte & 1) == 0) {
                ReceiveRegularPacket<isIPV6>(connection_index, packet_data, packet_bytes);
            }
            else {
                ReceiveFaragmentedPacket(connection_index, packet_data, packet_bytes);
            }
        }

        std::span<std::uint16_t, acknowledge_buffer_size> GetAcknowledgements(int connection_index, int& number_of_acknowledgements) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            number_of_acknowledgements = m_number_of_acknowledgements[connection_index];
            return std::span<std::uint16_t, acknowledge_buffer_size>(&m_acknowledgments[connection_index * acknowledge_buffer_size], acknowledge_buffer_size);
        }

        void ClearAcknowledgements(int connection_index, int cleared_acknowledgements) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            m_number_of_acknowledgements[connection_index] -= cleared_acknowledgements;
        }

        std::uint16_t GetNextPacketSequence(int connection_index) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            return m_sequence[connection_index];
        }

// Sections for managing code placement in file, only for development purposes e.g. for convenient folding inside an IDE.
#ifndef __rUDP_STATISTICS_FUNCTIONS__
        void CalculateMinMaxRoundTripTime() {
            for (int connection_index = 0; connection_index < max_connection_count; ++connection_index) {
                if (m_connection_manager->GetConnectionManager()->IsConnectionConnected(connection_index)) {
                    float min_rtt = 10000.0f;
                    float max_rtt = 0.0f;
                    float sum_rtt = 0.0f;
                    int count = 0;
                    for (int i = 0; i < ROUND_TRIP_TIME_HISTORY_SIZE; ++i) {
                        const float rtt = m_round_trip_time_history_buffer[connection_index * ROUND_TRIP_TIME_HISTORY_SIZE + i];
                        if (rtt >= 0.0f) {
                            if (rtt < min_rtt) {
                                min_rtt = rtt;
                            }
                            if (rtt > max_rtt) {
                                max_rtt = rtt;
                            }
                            sum_rtt += rtt;
                            ++count;
                        }
                    }
                    if (min_rtt == 10000.0f) {
                        min_rtt = 0.0f;
                    }
                    m_round_trip_time_minimum[connection_index] = min_rtt;
                    m_round_trip_time_maximum[connection_index] = max_rtt;
                    if (count > 0) {
                        m_round_trip_time_average[connection_index] = sum_rtt / (float)count;
                    }
                    else {
                        m_round_trip_time_average[connection_index] = 0.0f;
                    }
                }
            }
        }

        void CalculateJitter() {
            for (int connection_index = 0; connection_index < max_connection_count; ++connection_index) {
                if (m_connection_manager->GetConnectionManager()->IsConnectionConnected(connection_index)) {
                    float sum = 0.0f;
                    float sum_std = 0.0f;
                    float max = 0.0f;
                    int count = 0;
                    for (int i = 0; i < ROUND_TRIP_TIME_HISTORY_SIZE; ++i) {
                        if (m_round_trip_time_history_buffer[connection_index * ROUND_TRIP_TIME_HISTORY_SIZE + i] >= 0.0f) {
                            float difference = (m_round_trip_time_history_buffer[connection_index * ROUND_TRIP_TIME_HISTORY_SIZE + i] - m_round_trip_time_minimum[connection_index]);
                            float deviation = (m_round_trip_time_history_buffer[connection_index * ROUND_TRIP_TIME_HISTORY_SIZE + i] - m_round_trip_time_average[connection_index]);
                            sum += difference;
                            if (difference > max) {
                                max = difference;
                            }
                            sum_std += deviation * deviation;
                            ++count;
                        }
                    }
                    if (count > 0) {
                        m_average_jitter_from_minimum_rtt[connection_index] = sum / (float)count;
                        m_std_jitter_from_average_rtt[connection_index] = (float)std::pow(sum_std / (float)count, 0.5f);
                    }
                    else {
                        m_average_jitter_from_minimum_rtt[connection_index] = 0.0f;
                        m_std_jitter_from_average_rtt[connection_index] = 0.0f;
                    }
                    m_max_jitter_from_minimum_rtt[connection_index] = max;
                }
            }
        }

        void CalculatePacketLoss() {
            for (int connection_index = 0; connection_index < max_connection_count; ++connection_index) {
                if (m_connection_manager->GetConnectionManager()->IsConnectionConnected(connection_index)) {
                    uint32_t base_sequence = (m_sent_packets.GetSequence(connection_index) - sent_packet_buffer_size + 1) + std::numeric_limits<uint16_t>::max();
                    int num_sent = 0;
                    int num_dropped = 0;
                    for (int i = 0; i < sent_packet_buffer_size / 2; ++i) {
                        uint16_t sequence = static_cast<uint16_t>(base_sequence + i);

                        SentPacketData* sent_packet_data = m_sent_packets.Find(connection_index, sequence);
                        if (sent_packet_data) {
                            ++num_sent;
                            if (!sent_packet_data->acked) {
                                ++num_dropped;
                            }
                        }
                    }
                    if (num_sent > 0) {
                        float packet_loss = ((float)num_dropped) / ((float)num_sent) * 100.0f;
                        if (fabs(m_packet_loss[connection_index] - packet_loss) > 0.00001) {
                            m_packet_loss[connection_index] += (packet_loss - m_packet_loss[connection_index]) * PACKET_LOSS_SMOOTHING_FACTOR;
                        }
                        else {
                            m_packet_loss[connection_index] = packet_loss;
                        }
                    }
                    else {
                        m_packet_loss[connection_index] = 0.0f;
                    }
                }
            }
        }

        void CalculateSentBandwidth() {
            for (int connection_index = 0; connection_index < max_connection_count; ++connection_index) {
                if (m_connection_manager->GetConnectionManager()->IsConnectionConnected(connection_index)) {
                    uint32_t base_sequence = (m_sent_packets.GetSequence(connection_index) - sent_packet_buffer_size + 1) + std::numeric_limits<uint16_t>::max();
                    int bytes_sent = 0;
                    int acknowledged_bytes_sent = 0;
                    std::uint64_t start_time = std::numeric_limits<std::uint64_t>::max();
                    std::uint64_t finish_time = 0;
                    std::uint64_t acknowledged_start_time = std::numeric_limits<std::uint64_t>::max();
                    std::uint64_t acknowledged_finish_time = 0;
                    for (int i = 0; i < sent_packet_buffer_size / 2; ++i) {
                        uint16_t sequence = static_cast<uint16_t>(base_sequence + i);
                        SentPacketData* sent_packet_data = m_sent_packets.Find(connection_index, sequence);
                        if (!sent_packet_data) {
                            continue;
                        }
                        if (sent_packet_data->acked) {
                            acknowledged_bytes_sent += sent_packet_data->packet_bytes;
                            if (sent_packet_data->time < acknowledged_start_time) {
                                acknowledged_start_time = sent_packet_data->time;
                            }
                            if (sent_packet_data->time > acknowledged_finish_time) {
                                acknowledged_finish_time = sent_packet_data->time;
                            }
                        }
                        bytes_sent += sent_packet_data->packet_bytes;
                        if (sent_packet_data->time < start_time) {
                            start_time = sent_packet_data->time;
                        }
                        if (sent_packet_data->time > finish_time) {
                            finish_time = sent_packet_data->time;
                        }
                    }
                    if (start_time != std::numeric_limits<float>::max() && finish_time != 0.0) {
                        float sent_bandwidth_kbps = (float)(((double)bytes_sent) / (finish_time - start_time) * 8.0f / 1000.0f);
                        if (fabs(m_sent_bandwidth_kbps[connection_index] - sent_bandwidth_kbps) > 0.00001) {
                            m_sent_bandwidth_kbps[connection_index] += (sent_bandwidth_kbps - m_sent_bandwidth_kbps[connection_index]) * BANDWIDTH_SMOOTHING_FACTOR * 1000;
                        }
                        else {
                            m_sent_bandwidth_kbps[connection_index] = sent_bandwidth_kbps * 1000;
                        }
                    }
                    if (acknowledged_start_time != std::numeric_limits<float>::max() && acknowledged_finish_time != 0.0) {
                        float acked_bandwidth_kbps = (float)(((double)acknowledged_bytes_sent) / (acknowledged_finish_time - acknowledged_start_time) * 8.0f / 1000.0f);
                        if (fabs(m_acknowledged_bandwidth_kbps[connection_index] - acked_bandwidth_kbps) > 0.00001) {
                            m_acknowledged_bandwidth_kbps[connection_index] += (acked_bandwidth_kbps - m_acknowledged_bandwidth_kbps[connection_index]) * BANDWIDTH_SMOOTHING_FACTOR * 1000;
                        }
                        else {
                            m_acknowledged_bandwidth_kbps[connection_index] = acked_bandwidth_kbps * 1000;
                        }
                    }
                }
            }
        }

        void CalculateReceivedBandwidth() {
            for (int connection_index = 0; connection_index < max_connection_count; ++connection_index) {
                if (m_connection_manager->GetConnectionManager()->IsConnectionConnected(connection_index)) {
                    uint32_t base_sequence = (m_received_packets.GetSequence(connection_index) - received_packets_buffer_size + 1) + std::numeric_limits<uint16_t>::max();
                    int bytes_sent = 0;
                    std::uint64_t start_time = std::numeric_limits<std::uint64_t>::max();
                    std::uint64_t finish_time = 0;
                    for (int i = 0; i < received_packets_buffer_size / 2; ++i) {
                        uint16_t sequence = static_cast<uint16_t>(base_sequence + i);
                        ReceivedPacketData* received_packet_data = m_received_packets.Find(connection_index, sequence);
                        if (!received_packet_data) {
                            continue;
                        }
                        bytes_sent += received_packet_data->packet_bytes;
                        if (received_packet_data->time < start_time) {
                            start_time = received_packet_data->time;
                        }
                        if (received_packet_data->time > finish_time) {
                            finish_time = received_packet_data->time;
                        }
                    }
                    if (start_time != std::numeric_limits<std::uint64_t>::max() && finish_time != 0.0) {
                        float received_bandwidth_kbps = (float)(((double)bytes_sent) / (finish_time - start_time) * 8.0f / 1000.0f);
                        if (fabs(m_received_bandwidth_kbps[connection_index] - received_bandwidth_kbps) > 0.00001) {
                            m_received_bandwidth_kbps[connection_index] += (received_bandwidth_kbps - m_received_bandwidth_kbps[connection_index]) * BANDWIDTH_SMOOTHING_FACTOR * 1000;
                        }
                        else {
                            m_received_bandwidth_kbps[connection_index] = received_bandwidth_kbps * 1000;
                        }
                    }
                }
            }
        }

        void CalculateNetworkStatistics() {
            /// \todo maybe make this only happen every second?

            CalculateMinMaxRoundTripTime();

            CalculateJitter();

            CalculatePacketLoss();

            CalculateSentBandwidth();

            CalculateReceivedBandwidth();
        }

        float GetRoundTripTime(int connection_index) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            return m_round_trip_time[connection_index];
        }

        void GetBandwidth(int connection_index, float* sent_bandwidth_kbps, float* received_bandwidth_kbps, float* acked_bandwidth_kbps) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            ASSERT_CRASH(sent_bandwidth_kbps);
            ASSERT_CRASH(acked_bandwidth_kbps);
            ASSERT_CRASH(received_bandwidth_kbps);
            *sent_bandwidth_kbps = m_sent_bandwidth_kbps[connection_index];
            *received_bandwidth_kbps = m_received_bandwidth_kbps[connection_index];
            *acked_bandwidth_kbps = m_acknowledged_bandwidth_kbps[connection_index];
        }

        float GetPacketLoss(int connection_index) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            return m_packet_loss[connection_index];
        }

        float GetMinimumRoundTripTime(int connection_index) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            return m_round_trip_time_minimum[connection_index];
        }

        float GetMaximumRoundTripTime(int connection_index) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            return m_round_trip_time_maximum[connection_index];
        }

        float GetAverageRoundTripTime(int connection_index) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            return m_round_trip_time_average[connection_index];
        }

        float GetAverageJitterFromMinimumRoundTripTime(int connection_index) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            return m_average_jitter_from_minimum_rtt[connection_index];
        }

        float GetMaximumJitterFromMinimumRoundTripTime(int connection_index) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            return m_max_jitter_from_minimum_rtt[connection_index];
        }

        float GetStandardDeviationJitterFromAverageRoundTripTime(int connection_index) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            return m_std_jitter_from_average_rtt[connection_index];
        }

        const std::span<std::uint64_t, CounterTypes::MAX> GetCounters(int connection_index) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            return std::span<std::uint64_t, CounterTypes::MAX>(&m_counters[connection_index * CounterTypes::MAX], CounterTypes::MAX);
        }
#endif
    private:
        void StoreFragmentData(FragmentReassemblyData<max_number_of_fragments>* reassembly_data, std::uint16_t sequence, std::uint16_t ack, std::uint32_t ack_bits, int fragment_id, int fragment_size, std::byte* fragment_data, int fragment_bytes) {
            if (fragment_id == 0) {
                std::array<std::byte, MAX_RUDP_HEADER_BYTES> packet_header;

                packet_header.fill(std::byte(0x00));

                reassembly_data->packet_header_bytes = WritePacketHeader(packet_header.data(), sequence, ack, ack_bits);

                (void)std::copy_n(packet_header.data(), reassembly_data->packet_header_bytes, reassembly_data->packet_data + MAX_RUDP_HEADER_BYTES - reassembly_data->packet_header_bytes);

                fragment_data += reassembly_data->packet_header_bytes;
                fragment_bytes -= reassembly_data->packet_header_bytes;
            }

            if (fragment_id == reassembly_data->num_fragments_total - 1) {
                reassembly_data->packet_bytes = (reassembly_data->num_fragments_total - 1) * fragment_size + fragment_bytes;
            }

            (void)std::copy_n(fragment_data, fragment_bytes, reassembly_data->packet_data + MAX_RUDP_HEADER_BYTES + fragment_id * fragment_size);
        }

        template <bool isIPV6>
        void ReceiveRegularPacket(int connection_index, std::byte* packet_data, int packet_bytes) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            ASSERT_CRASH(packet_data != nullptr);
            ASSERT_CRASH(packet_bytes > 0);
            ++m_counters[connection_index * CounterTypes::MAX + CounterTypes::PacketsReceived];

            uint16_t sequence;
            uint16_t ack;
            uint32_t ack_bits;
            int packet_header_bytes = ReadPacketHeader(packet_data, packet_bytes, sequence, ack, ack_bits);
            if (packet_header_bytes < 0) {
                CORE_DEBUG("[rUDPConnetion] Ignoring invalid packet. Could not read packet header, connection index {}", connection_index);
                ++m_counters[CounterTypes::InvalidPackets];
                return;
            }

            ASSERT_CRASH(packet_header_bytes <= packet_bytes);

            int packet_payload_bytes = packet_bytes - packet_header_bytes;

            if (packet_payload_bytes > max_packet_size) {
                CORE_ERROR("[rUDPConnetion] Packet too large to receive. Packet is at {} bytes, maximum is {}, connection index {}", packet_payload_bytes, max_packet_size, connection_index);
                ++m_counters[connection_index * CounterTypes::MAX + CounterTypes::OversizedReceivePackets];
                return;
            }

            if (!m_received_packets.TestInsert(connection_index, sequence)) {
                CORE_DEBUG("[rUDPConnetion] Ignoring stale packet sequnce {}, connection index {}", sequence, connection_index);
                ++m_counters[connection_index * CounterTypes::MAX + CounterTypes::StalePackets];
                return;
            }

            CORE_DEBUG("[rUDPConnetion] Processing packet sequence {}, connection index {}", sequence, connection_index);


            if (m_connection_manager->ProcessDeserialised_rUDPPacket(connection_index, sequence, packet_data + packet_header_bytes, packet_bytes - packet_header_bytes)) {
                CORE_DEBUG("[rUDPConnetion] Process packet sequence {} successful, connection index {}", sequence, connection_index);

                ReceivedPacketData* received_packet_data = m_received_packets.Insert(connection_index, sequence);

                m_fragment_reassembly.AdvanceSequenceWithCleanup(connection_index, sequence, [&](FragmentReassemblyData<max_number_of_fragments>* faragment) {
                    Global::GMemory->Release(faragment->packet_data);
                });

                ASSERT_CRASH(received_packet_data != nullptr);

                received_packet_data->time = m_connection_manager->GetTime();
                if constexpr (isIPV6) {
                    received_packet_data->packet_bytes = IP_HEADER_SIZE_IPV6 + packet_bytes;
                }
                else {
                    received_packet_data->packet_bytes = IP_HEADER_SIZE_IPV4 + packet_bytes;
                }

                for (int i = 0; i < 32; ++i) {
                    if (ack_bits & 1) {
                        uint16_t ack_sequence = ack - (static_cast<uint16_t>(i));

                        SentPacketData* sent_packet_data = m_sent_packets.Find(connection_index, ack_sequence);

                        if (sent_packet_data && !sent_packet_data->acked && m_number_of_acknowledgements[connection_index] < acknowledge_buffer_size) {
                            CORE_DEBUG("[rUDPConnetion] Acknowledged packet sequence {}, connection index {}", ack_sequence, connection_index);
                            m_acknowledgments[connection_index * acknowledge_buffer_size + m_number_of_acknowledgements[connection_index]++] = ack_sequence;
                            ++m_counters[connection_index * CounterTypes::MAX + CounterTypes::PacketsAcknowledged];
                            sent_packet_data->acked = 1;

                            const float round_trip_time = (float)(m_connection_manager->GetTime() - sent_packet_data->time) * 1000.0f;
                            ASSERT_CRASH(round_trip_time >= 0.0);
                            int index = connection_index * ROUND_TRIP_TIME_HISTORY_SIZE + (ack_sequence % ROUND_TRIP_TIME_HISTORY_SIZE);

                            m_round_trip_time_history_buffer[index] = round_trip_time;
                            if ((m_round_trip_time[connection_index] == 0.0f && round_trip_time > 0.0f) || fabs(m_round_trip_time[connection_index] - round_trip_time) < 0.00001) {
                                m_round_trip_time[connection_index] = round_trip_time;
                            }
                            else {
                                m_round_trip_time[connection_index] += (round_trip_time - m_round_trip_time[connection_index]) * ROUND_TRIP_TIME_SMOOTHING_FACTOR;
                            }
                        }
                    }
                    ack_bits >>= 1;
                }
            }
            else {
                CORE_DEBUG("[rUDPConnetion] Packet processing failed, connection index {} ", connection_index);
            }
        }

        void ReceiveFaragmentedPacket(int connection_index, std::byte* packet_data, int packet_bytes) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            ASSERT_CRASH(packet_data != nullptr);
            ASSERT_CRASH(packet_bytes > 0);
            int fragment_id;
            int num_fragments;
            int fragment_bytes;

            uint16_t sequence;
            uint16_t ack;
            uint32_t ack_bits;

            int fragment_header_bytes = ReadFragmentHeader<max_number_of_fragments, fragment_size>(packet_data,
                packet_bytes,
                fragment_id,
                num_fragments,
                fragment_bytes,
                sequence,
                ack,
                ack_bits);

            if (fragment_header_bytes < 0) {
                CORE_DEBUG("[rUDPConnetion] Ignoring invalid fragment. Could not read fragment header, connection index {}.", connection_index);
                ++m_counters[connection_index * CounterTypes::MAX + CounterTypes::InvalidFragmantedPackets];
                return;
            }

            FragmentReassemblyData<max_number_of_fragments>* reassembly_data = m_fragment_reassembly.Find(connection_index, sequence);

            if (!reassembly_data) {
                reassembly_data = m_fragment_reassembly.InsertWithCleanup(
                    connection_index, sequence, [&](FragmentReassemblyData<max_number_of_fragments>* faragment) {
                        Global::GMemory->Release(faragment->packet_data);
                    });

                if (!reassembly_data) {
                    CORE_DEBUG("[rUDPConnetion] Ignoring invalid fragment. could not insert in reassembly buffer (stale), connection_index {}", connection_index);
                    ++m_counters[connection_index * CounterTypes::MAX + CounterTypes::InvalidFragmantedPackets];
                    return;
                }

                m_received_packets.AdvanceSequence(connection_index, sequence);

                int packet_buffer_size = MAX_RUDP_HEADER_BYTES + num_fragments * fragment_size;

                reassembly_data->sequence = sequence;
                reassembly_data->ack = 0;
                reassembly_data->ack_bits = 0;
                reassembly_data->num_fragments_received = 0;
                reassembly_data->num_fragments_total = num_fragments;
                reassembly_data->packet_data = static_cast<std::byte*>(Global::GMemory->Allocate(packet_buffer_size));
                reassembly_data->packet_bytes = 0;
                reassembly_data->fragment_received.fill(0);
            }

            if (num_fragments != (int)reassembly_data->num_fragments_total) {
                CORE_DEBUG("[rUDPConnetion] Ignoring invalid fragment. Fragment count mismatch. Expected {}, got {}, connection index {}", (int)reassembly_data->num_fragments_total, num_fragments, connection_index);
                ++m_counters[connection_index * CounterTypes::MAX + CounterTypes::InvalidFragmantedPackets];
                return;
            }

            if (reassembly_data->fragment_received[fragment_id]) {
                CORE_DEBUG("[rUDPConnetion] Ignoring fragment {} of packet {}. Fragment already received, connection_index {}", fragment_id, sequence, connection_index);
                return;
            }

            reassembly_data->num_fragments_received++;
            reassembly_data->fragment_received[fragment_id] = 1;

            CORE_DEBUG("[rUDPConnetion] Received fragment {} of packet {} ({}/{}), connection index {}", fragment_id, sequence, reassembly_data->num_fragments_received, num_fragments, connection_index);

            StoreFragmentData(reassembly_data,
                sequence,
                ack,
                ack_bits,
                fragment_id,
                fragment_size,
                packet_data + fragment_header_bytes,
                packet_bytes - fragment_header_bytes);

            if (reassembly_data->num_fragments_received == reassembly_data->num_fragments_total) {
                CORE_DEBUG("[rUDPConnetion] Completed reassembly of packet sequence {}, connection index {}", sequence, connection_index);

                ReliableReceivePacket(connection_index, reassembly_data->packet_data + MAX_RUDP_HEADER_BYTES - reassembly_data->packet_header_bytes,
                    reassembly_data->packet_header_bytes + reassembly_data->packet_bytes);

                m_fragment_reassembly.RemoveWithCleanup(connection_index, sequence, [&](FragmentReassemblyData<max_number_of_fragments>* faragment) {
                    Global::GMemory->Release(faragment->packet_data);
                });
            }
            ++m_counters[connection_index * CounterTypes::MAX + CounterTypes::FragmantedPacketsReceived];
        }

        alignas(std::hardware_destructive_interference_size) std::array<std::uint16_t, acknowledge_buffer_size * max_connection_count> m_acknowledgments;
        alignas(std::hardware_destructive_interference_size) std::array<std::uint16_t, max_connection_count> m_sequence;         // < It tells always the next sequence until we are sending something, in that case it is the current sequence
        alignas(std::hardware_destructive_interference_size) std::array<int, max_connection_count> m_number_of_acknowledgements; //< It tells how many sequence numbers are stored in m_acknowledgments, that hasn't been processed yet

        alignas(std::hardware_destructive_interference_size) std::array<float, max_connection_count> m_round_trip_time;
        alignas(std::hardware_destructive_interference_size) std::array<float, max_connection_count * ROUND_TRIP_TIME_HISTORY_SIZE> m_round_trip_time_history_buffer;
        alignas(std::hardware_destructive_interference_size) std::array<float, max_connection_count> m_round_trip_time_minimum;
        alignas(std::hardware_destructive_interference_size) std::array<float, max_connection_count> m_round_trip_time_maximum;
        alignas(std::hardware_destructive_interference_size) std::array<float, max_connection_count> m_round_trip_time_average;
        alignas(std::hardware_destructive_interference_size) std::array<float, max_connection_count> m_average_jitter_from_minimum_rtt;
        alignas(std::hardware_destructive_interference_size) std::array<float, max_connection_count> m_max_jitter_from_minimum_rtt;
        alignas(std::hardware_destructive_interference_size) std::array<float, max_connection_count> m_std_jitter_from_average_rtt;
        alignas(std::hardware_destructive_interference_size) std::array<float, max_connection_count> m_packet_loss;
        alignas(std::hardware_destructive_interference_size) std::array<float, max_connection_count> m_sent_bandwidth_kbps;
        alignas(std::hardware_destructive_interference_size) std::array<float, max_connection_count> m_received_bandwidth_kbps;
        alignas(std::hardware_destructive_interference_size) std::array<float, max_connection_count> m_acknowledged_bandwidth_kbps;

        ReliableBuffer<SentPacketData, sent_packet_buffer_size, max_connection_count> m_sent_packets;
        ReliableBuffer<ReceivedPacketData, received_packets_buffer_size, max_connection_count> m_received_packets;
        ReliableBuffer<FragmentReassemblyData<max_number_of_fragments>, fragment_reassembly_buffer_size, max_connection_count> m_fragment_reassembly;

        alignas(std::hardware_destructive_interference_size) std::array<std::uint64_t, CounterTypes::MAX * max_connection_count> m_counters;
        class_connection_manager* m_connection_manager;
    };
}