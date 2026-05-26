#include "BundleBFSTracker.h"
#include "Conversion.h"

/**
 * We can make use of one of the trackers for event structures like bundle and stable ESs that are more expressive.
 * However the bundle tracker would be more effecient as it would be similar wrt EventState inner structure, while
 * the stable tracker would have more complex event state structure.
 */
template <typename Node, class EnablersGetter, class SuccessorGetter, class InhibitedGetter>
    requires EventRangeGetter<EnablersGetter, Node> &&
             EventRangeGetter<SuccessorGetter, Node> &&
             EventRangeGetter<InhibitedGetter, Node>
struct PrimeBFSTracker : BundleBFSTracker<
                             Node,
                             PrimeToBundleCausalityAdaptor<Node, EnablersGetter>,
                             SuccessorGetter,
                             InhibitedGetter>
{

    PrimeBFSTracker(EnablersGetter e, SuccessorGetter s, InhibitedGetter i)
        : BundleBFSTracker<
              Node,
              PrimeToBundleCausalityAdaptor<Node, EnablersGetter>,
              SuccessorGetter,
              InhibitedGetter>(wrapPrimeAsBundle<Node>(std::move(e)), std::move(s), std::move(i)) {}
};