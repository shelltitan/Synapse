#include <Area/HeapArea.hpp>

#include <cstddef>
#include <cstdlib>

namespace Synapse::Memory::Area {
    HeapArea::HeapArea(const std::size_t size) noexcept {
        m_start = static_cast<std::byte*>(std::malloc(size * sizeof(std::byte)));
        m_end = m_start + size;
    }
    HeapArea::~HeapArea() noexcept {
        free(m_start);
    }
}
