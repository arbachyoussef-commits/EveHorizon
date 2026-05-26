#include "BundleKhanBFSTracker.h"
#include "Conversion.h"


template <typename Node, class EnablersGetter, class SuccessorGetter, class InhibitedGetter>
requires EventRangeGetter<EnablersGetter, Node> &&
         EventRangeGetter<SuccessorGetter, Node> &&
         EventRangeGetter<InhibitedGetter, Node>
struct PrimeKhanBFSTracker : 
        BundleKhanBFSTracker<
            Node, 
            PrimeToBundleCausalityAdaptor<Node, EnablersGetter>, 
            SuccessorGetter, 
            InhibitedGetter
        > {

    PrimeKhanBFSTracker(EnablersGetter e, SuccessorGetter s, InhibitedGetter i)
        : BundleKhanBFSTracker<
            Node, 
            PrimeToBundleCausalityAdaptor<Node, EnablersGetter>, 
            SuccessorGetter, 
            InhibitedGetter
        >(wrapPrimeAsBundle<Node>(std::move(e)), std::move(s), std::move(i)) {}
};