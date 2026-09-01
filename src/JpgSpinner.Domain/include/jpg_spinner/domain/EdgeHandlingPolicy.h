#pragma once

namespace jpg_spinner::domain
{
/// Selects how a coefficient transform handles a source edge that ends in
/// a partial interleaved minimum coded unit (iMCU). Trimming is never an
/// implicit recovery behavior because it permanently discards pixels.
enum class EdgeHandlingPolicy
{
    RequirePerfectCoefficientTransform,
    TrimPartialMinimumCodedUnits,
};
} // namespace jpg_spinner::domain
