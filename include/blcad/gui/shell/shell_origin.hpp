#pragma once

#include "blcad/core/result.hpp"
#include "blcad/gui/gui_document_session.hpp"

namespace blcad::gui {

// GUI Shell Reset MVP-9R Block 133: seeds a freshly created part document with
// the origin geometry — the three principal datum planes (datum.xy / datum.xz
// / datum.yz) and the three origin datum axes (datum.axis.x / .y / .z). The
// ids pair with the "Ursprung" browser folder in GuiDocumentBrowser. Runs as
// one part transaction; fails closed when the session holds no part document
// or any origin id already exists.
[[nodiscard]] Result<std::size_t> seed_origin_geometry(GuiDocumentSession& session);

} // namespace blcad::gui
