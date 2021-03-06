#include "stdafx.h"
#include "IcData.h"

IcDataOperationData IcOpData[IcMaximumOpcode] =
{
    { (IcOpcode)0 },
    { IcOpMov, "Mov", IcSelectorRR, true },
    { IcOpCvtC, "CvtC", IcSelectorRR, true },
    { IcOpCvtUC, "CvtUC", IcSelectorRR, true },
    { IcOpCvtS, "CvtS", IcSelectorRR, true },
    { IcOpCvtUS, "CvtUS", IcSelectorRR, true },
    { IcOpCvtI, "CvtI", IcSelectorRR, true },
    { IcOpCvtUI, "CvtUI", IcSelectorRR, true },
    { IcOpCvtL, "CvtL", IcSelectorRR, true },
    { IcOpCvtUL, "CvtUL", IcSelectorRR, true },
    { IcOpCvtLL, "CvtLL", IcSelectorRR, true },
    { IcOpCvtULL, "CvtULL", IcSelectorRR, true },
    { IcOpCvtF, "CvtF", IcSelectorRR, true },
    { IcOpCvtD, "CvtD", IcSelectorRR, true },
    { IcOpCvtP, "CvtP", IcSelectorRR, true },
    { IcOpAdd, "Add", IcSelectorRRR, true },
    { IcOpAddI, "AddI", IcSelectorRCR, true },
    { IcOpSub, "Sub", IcSelectorRRR, true },
    { IcOpSubI, "SubI", IcSelectorRCR, true },
    { IcOpSubIR, "SubIR", IcSelectorRCR, true },
    { IcOpMul, "Mul", IcSelectorRRR, true },
    { IcOpMulI, "MulI", IcSelectorRCR, true },
    { IcOpMulU, "MulU", IcSelectorRRR, true },
    { IcOpMulUI, "MulUI", IcSelectorRCR, true },
    { IcOpDiv, "Div", IcSelectorRRR, true },
    { IcOpDivI, "DivI", IcSelectorRCR, true },
    { IcOpDivIR, "DivIR", IcSelectorRCR, true },
    { IcOpDivU, "DivU", IcSelectorRRR, true },
    { IcOpDivUI, "DivUI", IcSelectorRCR, true },
    { IcOpDivUIR, "DivUIR", IcSelectorRCR, true },
    { IcOpMod, "Mod", IcSelectorRRR, true },
    { IcOpModI, "ModI", IcSelectorRCR, true },
    { IcOpModU, "ModU", IcSelectorRRR, true },
    { IcOpModUI, "ModUI", IcSelectorRCR, true },
    { IcOpNot, "Not", IcSelectorRR, true },
    { IcOpAnd, "And", IcSelectorRRR, true },
    { IcOpAndI, "AndI", IcSelectorRCR, true },
    { IcOpOr, "Or", IcSelectorRRR, true },
    { IcOpOrI, "OrI", IcSelectorRCR, true },
    { IcOpXor, "Xor", IcSelectorRRR, true },
    { IcOpXorI, "XorI", IcSelectorRCR, true },
    { IcOpShl, "Shl", IcSelectorRRR, true },
    { IcOpShlU, "ShlU", IcSelectorRRR, true },
    { IcOpShr, "Shr", IcSelectorRRR, true },
    { IcOpShrU, "ShrU", IcSelectorRRR, true },
    { IcOpLd, "Ld", IcSelectorRR, true },
    { IcOpLdI, "LdI", IcSelectorCR, true },
    { IcOpLdF, "LdF", IcSelectorCR, true },
    { IcOpLdFA, "LdFA", IcSelectorR, true },
    { IcOpLdN, "LdN", IcSelectorR, true },
    { IcOpSt, "St", IcSelectorRR, false },
    { IcOpStI, "StI", IcSelectorCR, false },
    { IcOpSrt, "Srt", IcSelectorR, false },
    { IcOpCpyI, "CpyI", IcSelectorRRC, false },
    { IcOpJmp, "Jmp", IcSelectorB, false },
    { IcOpBrE, "BrE", IcSelectorRRBB, false },
    { IcOpBrL, "BrL", IcSelectorRRBB, false },
    { IcOpBrLE, "BrLE", IcSelectorRRBB, false },
    { IcOpBrB, "BrB", IcSelectorRRBB, false },
    { IcOpBrBE, "BrBE", IcSelectorRRBB, false },
    { IcOpBrEI, "BrEI", IcSelectorRCBB, false },
    { IcOpBrLI, "BrLI", IcSelectorRCBB, false },
    { IcOpBrLEI, "BrLEI", IcSelectorRCBB, false },
    { IcOpBrBI, "BrBI", IcSelectorRCBB, false },
    { IcOpBrBEI, "BrBEI", IcSelectorRCBB, false },
    { IcOpChE, "ChE", IcSelectorRRCCR, true },
    { IcOpChL, "ChL", IcSelectorRRCCR, true },
    { IcOpChLE, "ChLE", IcSelectorRRCCR, true },
    { IcOpChB, "ChB", IcSelectorRRCCR, true },
    { IcOpChBE, "ChBE", IcSelectorRRCCR, true },
    { IcOpChEI, "ChEI", IcSelectorRCCCR, true },
    { IcOpChLI, "ChLI", IcSelectorRCCCR, true },
    { IcOpChLEI, "ChLEI", IcSelectorRCCCR, true },
    { IcOpChBI, "ChBI", IcSelectorRCCCR, true },
    { IcOpChBEI, "ChBEI", IcSelectorRCCCR, true },
    { IcOpCall, "Call", IcSelectorRRSRX, false },
    { IcOpCallV, "CallV", IcSelectorRRSRX, true },
    { IcOpCallI, "CallI", IcSelectorCRSRX, false },
    { IcOpCallVI, "CallVI", IcSelectorCRSRX, true },
    { IcOpPhi, "Phi", IcSelectorRSRX, true },
    { IcOpEnd, "End", IcSelectorNone, false }
};
