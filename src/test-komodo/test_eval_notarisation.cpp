#include <cryptoconditions.h>
#include <gtest/gtest.h>

#include "cc/betprotocol.h"
#include "cc/eval.h"
#include "base58.h"
#include "core_io.h"
#include "key.h"
#include "main.h"
#include "script/cc.h"
#include "primitives/transaction.h"
#include "script/interpreter.h"
#include "script/serverchecker.h"

#include "testutils.h"

#include "komodo_structs.h"

#include <boost/filesystem.hpp>

int32_t komodo_notaries(uint8_t pubkeys[64][33],int32_t height,uint32_t timestamp);
void komodo_notarysinit(int32_t origheight,uint8_t pubkeys[64][33],int32_t num);
void komodo_init(int32_t height);
void komodo_statefname(char *fname,char *symbol,char *str);

extern knotaries_entry *Pubkeys;
extern std::map<std::string, std::string> mapArgs;

namespace TestEvalNotarisation {


    class EvalMock : public Eval
    {
        public:
            uint32_t nNotaries;
            uint8_t notaries[64][33];
            std::map<uint256, CTransaction> txs;
            std::map<uint256, CBlockIndex> blocks;

            int32_t GetNotaries(uint8_t pubkeys[64][33], int32_t height, uint32_t timestamp) const
            {
                memcpy(pubkeys, notaries, sizeof(notaries));
                return nNotaries;
            }

            bool GetTxUnconfirmed(const uint256 &hash, CTransaction &txOut, uint256 &hashBlock) const
            {
                auto r = txs.find(hash);
                if (r != txs.end()) {
                    txOut = r->second;
                    if (blocks.count(hash) > 0)
                        hashBlock = hash;
                    return true;
                }
                return false;
            }

            bool GetBlock(uint256 hash, CBlockIndex& blockIdx) const
            {
                auto r = blocks.find(hash);
                if (r == blocks.end()) return false;
                blockIdx = r->second;
                return true;
            }
    };

    //static auto noop = [&](CMutableTransaction &mtx){};
    static auto noop = [](CMutableTransaction &mtx){};


    template<typename Modifier>
        void SetupEval(EvalMock &eval, CMutableTransaction &notary, Modifier modify)
        {
            eval.nNotaries = komodo_notaries(eval.notaries, 780060, 1522946781);

            // make fake notary inputs
            notary.vin.resize(11);
            for (int i=0; i<notary.vin.size(); i++) {
                CMutableTransaction txIn;
                txIn.vout.resize(1);
                txIn.vout[0].scriptPubKey << VCH(eval.notaries[i*2], 33) << OP_CHECKSIG;
                notary.vin[i].prevout = COutPoint(txIn.GetHash(), 0);
                eval.txs[txIn.GetHash()] = CTransaction(txIn);
            }

            modify(notary);

            eval.txs[notary.GetHash()] = CTransaction(notary);
            eval.blocks[notary.GetHash()].SetHeight(780060);
            eval.blocks[notary.GetHash()].nTime = 1522946781;
        }


    // https://kmd.explorer.supernet.org/tx/5b8055d37cff745a404d1ae45e21ffdba62da7b28ed6533c67468d7379b20bae
    // inputs have been dropped
    static auto rawNotaryTx = "01000000000290460100000000002321020e46e79a2a8d12b9b5d12c7a91adb4e454edfae43c0a0cb805427d2ac7613fd9ac0000000000000000506a4c4dae8e0f3e6e5de498a072f5967f3c418c4faba5d56ac8ce17f472d029ef3000008f2e0100424f545300050ba773f0bc31da5839fc7cb9bd7b87f3b765ca608e5cf66785a466659b28880500000000000000";
    CTransaction notaryTx;
    static bool init = DecodeHexTx(notaryTx, rawNotaryTx);

    static uint256 proofTxHash = uint256S("37f76551a16093fbb0a92ee635bbd45b3460da8fd00cf7d5a6b20d93e727fe4c");
    static auto vMomProof = ParseHex("0303faecbdd4b3da128c2cd2701bb143820a967069375b2ec5b612f39bbfe78a8611978871c193457ab1e21b9520f4139f113b8d75892eb93ee247c18bccfd067efed7eacbfcdc8946cf22de45ad536ec0719034fb9bc825048fe6ab61fee5bd6e9aae0bb279738d46673c53d68eb2a72da6dbff215ee41a4d405a74ff7cd355805b");  // $ fiat/bots txMoMproof $proofTxHash

    /*
       TEST(TestEvalNotarisation, testGetNotarisation)
       {
       EvalMock eval;
       CMutableTransaction notary(notaryTx);
       SetupEval(eval, notary, noop);

       NotarisationData data;
    ASSERT_TRUE(eval.GetNotarisationData(notary.GetHash(), data));
    EXPECT_EQ(data.height, 77455);
    EXPECT_EQ(data.blockHash.GetHex(), "000030ef29d072f417cec86ad5a5ab4f8c413c7f96f572a098e45d6e3e0f8eae");
    EXPECT_STREQ(data.symbol, "BOTS");
    EXPECT_EQ(data.MoMDepth, 5);
    EXPECT_EQ(data.MoM.GetHex(), "88289b6566a48567f65c8e60ca65b7f3877bbdb97cfc3958da31bcf073a70b05");

    MoMProof proof;
    E_UNMARSHAL(vMomProof, ss >> proof);
    EXPECT_EQ(data.MoM, proof.branch.Exec(proofTxHash));
}


TEST(TestEvalNotarisation, testInvalidNotaryPubkey)
{
    EvalMock eval;
    CMutableTransaction notary(notaryTx);
    SetupEval(eval, notary, noop);

    memset(eval.notaries[10], 0, 33);

    NotarisationData data;
    ASSERT_FALSE(eval.GetNotarisationData(notary.GetHash(), data));
}
*/


TEST(TestEvalNotarisation, testInvalidNotarisationBadOpReturn)
{
    EvalMock eval;
    EVAL_TEST = &eval;
    CMutableTransaction notary(notaryTx);

    notary.vout[1].scriptPubKey = CScript() << OP_RETURN << 0;
    SetupEval(eval, notary, noop);

    NotarisationData data(0);
    ASSERT_FALSE(eval.GetNotarisationData(notary.GetHash(), data));
}


TEST(TestEvalNotarisation, testInvalidNotarisationTxNotEnoughSigs)
{
    EvalMock eval;
    EVAL_TEST = &eval;
    CMutableTransaction notary(notaryTx);

    SetupEval(eval, notary, [](CMutableTransaction &tx) {
        tx.vin.resize(10);
    });

    NotarisationData data(0);
    ASSERT_FALSE(eval.GetNotarisationData(notary.GetHash(), data));
}


TEST(TestEvalNotarisation, testInvalidNotarisationTxDoesntExist)
{
    EvalMock eval;
    EVAL_TEST = &eval;
    CMutableTransaction notary(notaryTx);

    SetupEval(eval, notary, noop);

    NotarisationData data(0);
    ASSERT_FALSE(eval.GetNotarisationData(uint256(), data));
}


TEST(TestEvalNotarisation, testInvalidNotarisationDupeNotary)
{
    EvalMock eval;
    EVAL_TEST = &eval;
    CMutableTransaction notary(notaryTx);

    SetupEval(eval, notary, [](CMutableTransaction &tx) {
        tx.vin[1] = tx.vin[3];
    });

    NotarisationData data(0);
    ASSERT_FALSE(eval.GetNotarisationData(notary.GetHash(), data));
}


TEST(TestEvalNotarisation, testInvalidNotarisationInputNotCheckSig)
{
    EvalMock eval;
    EVAL_TEST = &eval;
    CMutableTransaction notary(notaryTx);

    SetupEval(eval, notary, [&](CMutableTransaction &tx) {
        int i = 1;
        CMutableTransaction txIn;
        txIn.vout.resize(1);
        txIn.vout[0].scriptPubKey << VCH(eval.notaries[i*2], 33) << OP_RETURN;
        notary.vin[i].prevout = COutPoint(txIn.GetHash(), 0);
        eval.txs[txIn.GetHash()] = CTransaction(txIn);
    });

    NotarisationData data(0);
    ASSERT_FALSE(eval.GetNotarisationData(notary.GetHash(), data));
}

TEST(TestEvalNotarisation, testNotaryInit)
{
    // make an empty komodostate file
    boost::filesystem::path temp_path = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
    boost::filesystem::create_directories(temp_path);
    mapArgs["-datadir"] = temp_path.string();
    {
        boost::filesystem::path file = temp_path / "komodostate";
        std::ofstream komodostate(file.string());
        komodostate << "0" << std::endl;
    }
    // now we can get to testing. Load up the notaries from genesis
    EXPECT_EQ(Pubkeys, nullptr);
    komodo_init(0);
    boost::filesystem::remove_all(temp_path);
    EXPECT_NE(Pubkeys, nullptr);
    EXPECT_EQ(Pubkeys[0].height, 0);
    EXPECT_EQ(Pubkeys[0].numnotaries, 35);

    uint8_t new_key[33] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
            0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20};
    uint8_t new_notaries[64][33];
    memcpy(new_notaries[0], new_key, 33);

    // attempt to update with 1 key to an existing height
    komodo_notarysinit(0, new_notaries, 1);
    EXPECT_EQ(Pubkeys[0].height, 0);
    EXPECT_EQ(Pubkeys[0].numnotaries, 1);
    EXPECT_EQ(Pubkeys[0].Notaries->notaryid, 0);
    EXPECT_EQ(Pubkeys[0].Notaries->pubkey[0], 0x00);
    // that should push these keys to all heights above
    EXPECT_EQ(Pubkeys[1].Notaries->pubkey[0], 0x00);
    EXPECT_EQ(Pubkeys[2].Notaries->pubkey[0], 0x00);
    EXPECT_EQ(Pubkeys[3].Notaries->pubkey[0], 0x00);

    // add a new height with only 1 notary
    new_key[0] = 0x01;
    memcpy(new_notaries[0], new_key, 33);
    komodo_notarysinit(1, new_notaries, 1); // height of 1, 1 public key
    EXPECT_EQ(Pubkeys[1].height, KOMODO_ELECTION_GAP); // smart enough to bump the height to the next election cycle
    EXPECT_EQ(Pubkeys[1].numnotaries, 1);
    EXPECT_EQ(Pubkeys[1].Notaries->notaryid, 0);
    EXPECT_EQ(Pubkeys[1].Notaries->pubkey[0], 0x01);
    // that should push these keys to all heights above (but not below)
    EXPECT_EQ(Pubkeys[0].Notaries->pubkey[0], 0x00);
    EXPECT_EQ(Pubkeys[2].Notaries->pubkey[0], 0x01);
    EXPECT_EQ(Pubkeys[3].Notaries->pubkey[0], 0x01);
    EXPECT_EQ(Pubkeys[4].Notaries->pubkey[0], 0x01);

    // attempt to update with 1 key to a previous height
    new_key[0] = 0x02;
    memcpy(new_notaries[0], new_key, 33);
    komodo_notarysinit(0, new_notaries, 1);
    EXPECT_EQ(Pubkeys[0].height, 0);
    EXPECT_EQ(Pubkeys[0].numnotaries, 1);
    EXPECT_EQ(Pubkeys[0].Notaries->notaryid, 0);
    EXPECT_EQ(Pubkeys[0].Notaries->pubkey[0], 0x02);
    // that should not have changed anything above
    EXPECT_EQ(Pubkeys[1].numnotaries, 1);
    EXPECT_EQ(Pubkeys[1].Notaries->notaryid, 0);
    EXPECT_EQ(Pubkeys[1].Notaries->pubkey[0], 0x01);

    // add a new height with only 1 notary
    new_key[0] = 0x03;
    memcpy(new_notaries[0], new_key, 33);
    komodo_notarysinit(KOMODO_ELECTION_GAP + 1, new_notaries, 1); // height of 2001, 1 public key
    EXPECT_EQ(Pubkeys[2].height, KOMODO_ELECTION_GAP * 2); // smart enough to bump the height to the next election cycle
    EXPECT_EQ(Pubkeys[2].numnotaries, 1);
    EXPECT_EQ(Pubkeys[2].Notaries->notaryid, 0);
    EXPECT_EQ(Pubkeys[2].Notaries->pubkey[0], 0x03);
    EXPECT_EQ(Pubkeys[3].Notaries->pubkey[0], 0x03);
    EXPECT_EQ(Pubkeys[4].Notaries->pubkey[0], 0x03);

    // attempt to update with 1 key to a previous height. This should only change 1 key.
    new_key[0] = 0x04;
    memcpy(new_notaries[0], new_key, 33);
    komodo_notarysinit(0, new_notaries, 1);
    EXPECT_EQ(Pubkeys[0].height, 0);
    EXPECT_EQ(Pubkeys[0].numnotaries, 1);
    EXPECT_EQ(Pubkeys[0].Notaries->notaryid, 0);
    EXPECT_EQ(Pubkeys[0].Notaries->pubkey[0], 0x04);
    EXPECT_EQ(Pubkeys[1].Notaries->pubkey[0], 0x01);
    // that should not have changed the next height index
    EXPECT_EQ(Pubkeys[2].numnotaries, 1);
    EXPECT_EQ(Pubkeys[2].Notaries->notaryid, 0);
    EXPECT_EQ(Pubkeys[2].Notaries->pubkey[0], 0x03);
}


} /* namespace TestEvalNotarisation */
