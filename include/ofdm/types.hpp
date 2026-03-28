#pragma once

namespace ofdm {

enum class ModScheme { QPSK, QAM16, QAM64 };

int bits_per_symbol(ModScheme s);
const char* scheme_name(ModScheme s);

} // namespace ofdm
