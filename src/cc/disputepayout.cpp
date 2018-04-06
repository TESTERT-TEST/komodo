#include <cryptoconditions.h>

#include "hash.h"
#include "chain.h"
#include "version.h"
#include "komodo_cc.h"
#include "cc/eval.h"
#include "cc/betprotocol.h"
#include "primitives/transaction.h"


/*
 * Crypto-Condition EVAL method that resolves a dispute of a session
 *
 * IN: vm - AppVM virtual machine to verify states
 * IN: cond - CC EVAL node
 * IN: disputeTx - transaction attempting to resolve dispute
 * IN: nIn  - index of input of dispute tx
 *
 * disputeTx: attempt to resolve a dispute
 *
 *   in  0:      Spends Session TX first output, reveals DisputeHeader
 *   out 0:      OP_RETURN hash of payouts
 */
bool Eval::DisputePayout(AppVM &vm, const CC *cond, const CTransaction &disputeTx, unsigned int nIn)
{
    if (disputeTx.vout.size() == 0) return Invalid("no-vouts");

    // get payouts hash
    uint256 payoutHash;
    if (!GetOpReturnHash(disputeTx.vout[0].scriptPubKey, payoutHash))
        return Invalid("invalid-payout-hash");

    // load dispute header
    DisputeHeader disputeHeader;
    std::vector<unsigned char> headerData(
            cond->paramsBin, cond->paramsBin+cond->paramsBinLength);
    if (!CheckDeserialize(headerData, disputeHeader))
        return Invalid("invalid-dispute-header");

    // ensure that enough time has passed
    {
        CTransaction sessionTx;
        CBlockIndex sessionBlock;
        
        // if unconformed its too soon
        if (!GetTxConfirmed(disputeTx.vin[0].prevout.hash, sessionTx, sessionBlock))
            return Error("couldnt-get-parent");

        if (GetCurrentHeight() < sessionBlock.nHeight + disputeHeader.waitBlocks)
            return Invalid("dispute-too-soon");  // Not yet
    }

    // get spends
    std::vector<CTransaction> spends;
    if (!GetSpendsConfirmed(disputeTx.vin[0].prevout.hash, spends))
        return Error("couldnt-get-spends");

    // verify result from VM
    int maxLength = -1;
    uint256 bestPayout;
    for (int i=1; i<spends.size(); i++)
    {
        std::vector<unsigned char> vmState;
        if (!spends[i].vout.size() > 0) continue;
        if (!GetOpReturnData(spends[i].vout[0].scriptPubKey, vmState)) continue;
        auto out = vm.evaluate(disputeHeader.vmParams, vmState);
        uint256 resultHash = SerializeHash(out.second);
        if (out.first > maxLength) {
            maxLength = out.first;
            bestPayout = resultHash;
        }
        // The below means that if for any reason there is a draw, the first dispute wins
        else if (out.first == maxLength) {
            if (bestPayout != payoutHash) {
                fprintf(stderr, "WARNING: VM has multiple solutions of same length\n");
                bestPayout = resultHash;
            }
        }
    }

    if (maxLength == -1) return Invalid("no-evidence");

    return bestPayout == payoutHash ? Valid() : Invalid("wrong-payout");
}
