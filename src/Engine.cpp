#include "ebm/Engine.hpp"
#include "ebm/Composer.hpp"
namespace ebm {
Phrase generate(const Config& config) { return v2::render(v2::compose(config)); }
const char* styleName(Style style) {
    switch (style) {
        case Style::MinimalBody: return "MINIMAL_BODY";
        case Style::ClassicEBM: return "CLASSIC_EBM";
        case Style::DarkElectro: return "DARK_ELECTRO";
    }
    return "INVALID";
}
} // namespace ebm
