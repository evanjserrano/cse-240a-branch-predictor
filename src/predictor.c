//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include "predictor.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

//
// TODO:Student Information
//
const char* studentName = "Evan Serrano";
const char* studentID = "A15543204";
const char* email = "e1serran@ucsd.edu";

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)

#define TRAIN(a, outcome)                                                      \
    switch (a)                                                                 \
    {                                                                          \
    case WN:                                                                   \
        a = (outcome == TAKEN) ? WT : SN;                                      \
        break;                                                                 \
    case SN:                                                                   \
        a = (outcome == TAKEN) ? WN : SN;                                      \
        break;                                                                 \
    case WT:                                                                   \
        a = (outcome == TAKEN) ? ST : WN;                                      \
        break;                                                                 \
    case ST:                                                                   \
        a = (outcome == TAKEN) ? ST : WT;                                      \
        break;                                                                 \
    default:                                                                   \
        printf("Warning: Undefined state of entry\n");                         \
    }

uint8_t PREDICT(uint8_t a)
{
    switch (a)
    {
    case WN:
        return NOTTAKEN;
    case SN:
        return NOTTAKEN;
    case WT:
        return TAKEN;
    case ST:
        return TAKEN;
    default:
        printf("Warning: Undefined state of entry\n");
        return NOTTAKEN;
    }
}

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char* bpName[4] = {"Static", "Gshare", "Tournament", "Custom"};

// define number of bits required for indexing the BHT here.
int ghistoryBits = 14; // Number of bits used for Global History
int bpType;            // Branch Prediction Type
int verbose;

// tournament
const int trn_pcBits = 11;        // PC register size (effective size for BP)
const int trn_ghr_bBits = 12;     // global bht index size
const int trn_ghr_cBits = 11;     // chooser index size
const int trn_local_phtBits = 10; // local predictor pht entry size
const int trn_local_bhtBits = 2;  // local predictor bht entry size
const int trn_global_bhtBits = 2; // global preictor bht entry size
const int trn_chooserBits = 3;    // chooser entry size

const int trn_local_phtSize = 1 << 11; // local predictor pht # entries
const int trn_global_bhtSize = 1 << 12;
const int trn_chooserSize = 1 << 11;
const int trn_local_bhtSize = 1 << 10;

// const int trn_local_phtSize = 1 << trn_pcBits;
// local predictor pht # entries
// const int trn_local_bhtSize = 1 << trn_local_phtBits;
// const int trn_global_bhtSize = 1 << trn_ghr_bBits;
// const int trn_chooserSize = 1 << trn_ghr_cBits;

// custom

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

//
// TODO: Add your own Branch Predictor data structures here
//
// gshare
uint8_t* bht_gshare;
uint64_t ghistory;

// tournament
uint16_t* trn_local_pht;
uint8_t* trn_local_bht;
uint8_t* trn_global_bht;
uint8_t* trn_chooser;

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// gshare functions
void init_gshare();
uint8_t gshare_predict(uint32_t pc);
void train_gshare(uint32_t pc, uint8_t outcome);
void cleanup_gshare();

// tournament functions
void init_tournament();
uint8_t tournament_predict(uint32_t pc);
void train_tournament(uint32_t pc, uint8_t outcome);
void cleanup_tournament();

// custom functions
void init_custom();
uint8_t custom_predict(uint32_t pc);
void train_custom(uint32_t pc, uint8_t outcome);
void cleanup_custom();

// Initialize the predictor
//

// gshare functions
void init_gshare()
{
    int bht_entries = 1 << ghistoryBits;
    bht_gshare = (uint8_t*)malloc(bht_entries * sizeof(uint8_t));
    int i = 0;
    for (i = 0; i < bht_entries; i++)
    {
        bht_gshare[i] = WN;
    }
    ghistory = 0;
}

uint8_t gshare_predict(uint32_t pc)
{
    // get lower ghistoryBits of pc
    uint32_t bht_entries = 1 << ghistoryBits;
    uint32_t pc_lower_bits = pc & (bht_entries - 1);
    uint32_t ghistory_lower_bits = ghistory & (bht_entries - 1);
    uint32_t index = pc_lower_bits ^ ghistory_lower_bits;
    switch (bht_gshare[index])
    {
    case WN:
        return NOTTAKEN;
    case SN:
        return NOTTAKEN;
    case WT:
        return TAKEN;
    case ST:
        return TAKEN;
    default:
        printf("Warning: Undefined state of entry in GSHARE BHT!\n");
        return NOTTAKEN;
    }
}

void train_gshare(uint32_t pc, uint8_t outcome)
{
    // get lower ghistoryBits of pc
    uint32_t bht_entries = 1 << ghistoryBits;
    uint32_t pc_lower_bits = pc & (bht_entries - 1);
    uint32_t ghistory_lower_bits = ghistory & (bht_entries - 1);
    uint32_t index = pc_lower_bits ^ ghistory_lower_bits;

    // Update state of entry in bht based on outcome
    switch (bht_gshare[index])
    {
    case WN:
        bht_gshare[index] = (outcome == TAKEN) ? WT : SN;
        break;
    case SN:
        bht_gshare[index] = (outcome == TAKEN) ? WN : SN;
        break;
    case WT:
        bht_gshare[index] = (outcome == TAKEN) ? ST : WN;
        break;
    case ST:
        bht_gshare[index] = (outcome == TAKEN) ? ST : WT;
        break;
    default:
        printf("Warning: Undefined state of entry in GSHARE BHT!\n");
    }

    // Update history register
    ghistory = ((ghistory << 1) | outcome);
}

void cleanup_gshare()
{
    free(bht_gshare);
}

// tournament functions
void init_tournament()
{
    trn_local_pht = (uint16_t*)malloc(trn_local_phtSize * sizeof(uint16_t));
    trn_local_bht = (uint8_t*)malloc(trn_local_bhtSize * sizeof(uint8_t));
    trn_global_bht = (uint8_t*)malloc(trn_global_bhtSize * sizeof(uint8_t));
    trn_chooser = (uint8_t*)malloc(trn_chooserSize * sizeof(uint8_t));

    memset(trn_local_pht, 0, trn_local_phtSize * sizeof(uint16_t));
    memset(trn_local_bht, WT, trn_local_bhtSize);
    memset(trn_global_bht, WT, trn_global_bhtSize);
    memset(trn_chooser, WT, trn_chooserSize);
}

uint8_t tournament_predict(uint32_t pc)
{
    uint8_t prediction;

    // choose local or global
    uint16_t ghr_c = ghistory & (trn_chooserSize - 1);
    uint16_t ghr_b = ghistory & (trn_global_bhtSize - 1);
    // uint8_t choice = PREDICT(trn_chooser[ghr]);
    uint8_t choice =
        trn_chooser[ghr_c] >> (trn_chooserBits - 1) ? TAKEN : NOTTAKEN;

    if (choice == TAKEN)
    {
        // global predictor (1)
        // get prediction based on ghr
        // return PREDICT(trn_global_bht[ghr]);
        return trn_global_bht[ghr_b] >> (trn_global_bhtBits - 1) ? TAKEN
                                                                 : NOTTAKEN;
    }
    else
    {
        // local predictor (0)
        uint16_t pc_idx = pc & (trn_local_phtSize - 1);
        // get pattern of branch
        uint16_t pat = (trn_local_pht[pc_idx]) & (trn_local_bhtSize - 1);
        // get prediction based on pattern
        // return PREDICT(trn_local_bht[pat]);
        return trn_local_bht[pat] >> (trn_local_bhtBits - 1) ? TAKEN : NOTTAKEN;
    }

    // return prediction ? TAKEN : NOTTAKEN;
}

void train_tournament(uint32_t pc, uint8_t outcome)
{
    uint8_t global_pred;
    uint8_t local_pred;
    uint8_t choice;

    // choose local or global
    uint16_t ghr_c = ghistory & (trn_chooserSize - 1);
    uint16_t ghr_b = ghistory & (trn_global_bhtSize - 1);
    // choice = PREDICT(trn_chooser[ghr]);
    choice = trn_chooser[ghr_c] >> (trn_chooserBits - 1) ? TAKEN : NOTTAKEN;

    // global predictor (1)
    // global_pred = PREDICT(trn_global_bht[ghr]);
    global_pred =
        trn_global_bht[ghr_b] >> (trn_global_bhtBits - 1) ? TAKEN : NOTTAKEN;

    // local predictor (0)
    uint16_t pc_idx = pc & (trn_local_phtSize - 1);
    uint16_t pat = (trn_local_pht[pc_idx]) & (trn_local_bhtSize - 1);
    // local_pred = PREDICT(trn_local_bht[pat]);
    local_pred =
        trn_local_bht[pat] >> (trn_local_bhtBits - 1) ? TAKEN : NOTTAKEN;

    // update tables
    // only update choice if predictions differ
    if (global_pred != local_pred)
    {
        // TRAIN(trn_chooser[ghr], (outcome == global_pred ? TAKEN : NOTTAKEN))
        if (global_pred == outcome)
        {
            trn_chooser[ghr_c] =
                MIN(trn_chooser[ghr_c] + 1, (1 << trn_chooserBits) - 1);
        }
        else if (local_pred == outcome)
        {
            trn_chooser[ghr_c] = MAX(trn_chooser[ghr_c], 1) - 1;
        }
    }

    // ghr and pattern table
    ghistory = ((ghistory << 1) | (outcome & 0x1));
    trn_local_pht[pc_idx] = ((trn_local_pht[pc_idx] << 1) | (outcome & 0x1));

    // branch history tables
    // TRAIN(trn_global_bht[ghr], outcome)
    if (outcome == TAKEN)
        trn_global_bht[ghr_b] =
            MIN(trn_global_bht[ghr_b] + 1, (1 << trn_global_bhtBits) - 1);
    else
        trn_global_bht[ghr_b] = MAX(trn_global_bht[ghr_b], 1) - 1;

    // TRAIN(trn_local_bht[pat], outcome)
    if (outcome == TAKEN)
        trn_local_bht[pat] =
            MIN(trn_local_bht[pat] + 1, (1 << trn_local_bhtBits) - 1);
    else
        trn_local_bht[pat] = MAX(trn_local_bht[pat], 1) - 1;
}

void cleanup_tournament()
{
    /*
    for (int i = 0; i < trn_local_bhtSize; i++)
    {
        uint16_t entry = trn_local_bht[i];
        printf("%03X: ", i);
        for (int j = 7; j >= 0; j--)
        {
            int bit = (entry >> j) & 0x1;
            printf("%i", bit);
        }
        printf("\n");
    }
    */
    free(trn_local_pht);
    free(trn_local_bht);
    free(trn_global_bht);
    free(trn_chooser);
}

// custom functions
void init_custom()
{
    trn_local_pht = (uint16_t*)malloc(trn_local_phtSize * sizeof(uint16_t));
    trn_local_bht = (uint8_t*)malloc(trn_local_bhtSize * sizeof(uint8_t));
    trn_global_bht = (uint8_t*)malloc(trn_global_bhtSize * sizeof(uint8_t));
    trn_chooser = (uint8_t*)malloc(trn_chooserSize * sizeof(uint8_t));

    memset(trn_local_pht, 0, trn_local_phtSize * sizeof(uint16_t));
    memset(trn_local_bht, WT, trn_local_bhtSize);
    memset(trn_global_bht, WT, trn_global_bhtSize);
    memset(trn_chooser, WT, trn_chooserSize);
}

uint8_t custom_predict(uint32_t pc)
{
    uint8_t prediction;

    // choose local or global
    uint16_t ghr_c = ghistory & (trn_chooserSize - 1);
    uint16_t ghr_b = (ghistory ^ pc) & (trn_global_bhtSize - 1);
    // uint8_t choice = PREDICT(trn_chooser[ghr]);
    uint8_t choice =
        trn_chooser[ghr_c] >> (trn_chooserBits - 1) ? TAKEN : NOTTAKEN;

    if (choice == TAKEN)
    {
        // global predictor (1)
        // get prediction based on ghr
        // return PREDICT(trn_global_bht[ghr]);
        return trn_global_bht[ghr_b] >> (trn_global_bhtBits - 1) ? TAKEN
                                                                 : NOTTAKEN;
    }
    else
    {
        // local predictor (0)
        uint16_t pc_idx = pc & (trn_local_phtSize - 1);
        // get pattern of branch
        uint16_t pat = (trn_local_pht[pc_idx]) & (trn_local_bhtSize - 1);
        // get prediction based on pattern
        // return PREDICT(trn_local_bht[pat]);
        return trn_local_bht[pat] >> (trn_local_bhtBits - 1) ? TAKEN : NOTTAKEN;
    }
}

void train_custom(uint32_t pc, uint8_t outcome)
{
    uint8_t global_pred;
    uint8_t local_pred;
    uint8_t choice;

    // choose local or global
    uint16_t ghr_c = (ghistory) & (trn_chooserSize - 1);
    uint16_t ghr_b = (ghistory ^ pc) & (trn_global_bhtSize - 1);
    // choice = PREDICT(trn_chooser[ghr]);
    choice = trn_chooser[ghr_c] >> (trn_chooserBits - 1) ? TAKEN : NOTTAKEN;

    // global predictor (1)
    // global_pred = PREDICT(trn_global_bht[ghr]);
    global_pred =
        trn_global_bht[ghr_b] >> (trn_global_bhtBits - 1) ? TAKEN : NOTTAKEN;

    // local predictor (0)
    uint16_t pc_idx = pc & (trn_local_phtSize - 1);
    uint16_t pat = (trn_local_pht[pc_idx]) & (trn_local_bhtSize - 1);
    // local_pred = PREDICT(trn_local_bht[pat]);
    local_pred =
        trn_local_bht[pat] >> (trn_local_bhtBits - 1) ? TAKEN : NOTTAKEN;

    // update tables
    // only update choice if predictions differ
    if (global_pred != local_pred)
    {
        // TRAIN(trn_chooser[ghr], (outcome == global_pred ? TAKEN : NOTTAKEN))
        if (global_pred == outcome)
        {
            trn_chooser[ghr_c] =
                MIN(trn_chooser[ghr_c] + 1, (1 << trn_chooserBits) - 1);
        }
        else if (local_pred == outcome)
        {
            trn_chooser[ghr_c] = MAX(trn_chooser[ghr_c], 1) - 1;
        }
    }

    // ghr and pattern table
    ghistory = ((ghistory << 1) | (outcome & 0x1));
    trn_local_pht[pc_idx] = ((trn_local_pht[pc_idx] << 1) | (outcome & 0x1));

    // branch history tables
    // TRAIN(trn_global_bht[ghr], outcome)
    if (outcome == TAKEN)
        trn_global_bht[ghr_b] =
            MIN(trn_global_bht[ghr_b] + 1, (1 << trn_global_bhtBits) - 1);
    else
        trn_global_bht[ghr_b] = MAX(trn_global_bht[ghr_b], 1) - 1;

    // TRAIN(trn_local_bht[pat], outcome)
    if (outcome == TAKEN)
        trn_local_bht[pat] =
            MIN(trn_local_bht[pat] + 1, (1 << trn_local_bhtBits) - 1);
    else
        trn_local_bht[pat] = MAX(trn_local_bht[pat], 1) - 1;
}

void init_predictor()
{
    switch (bpType)
    {
    case STATIC:
        break;
    case GSHARE:
        init_gshare();
        break;
    case TOURNAMENT:
        init_tournament();
        break;
    case CUSTOM:
        init_custom();
    default:
        break;
    }
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint8_t make_prediction(uint32_t pc)
{

    // Make a prediction based on the bpType
    switch (bpType)
    {
    case STATIC:
        return TAKEN;
    case GSHARE:
        return gshare_predict(pc);
    case TOURNAMENT:
        return tournament_predict(pc);
    case CUSTOM:
        return custom_predict(pc);
    default:
        break;
    }

    // If there is not a compatable bpType then return NOTTAKEN
    return NOTTAKEN;
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//

void train_predictor(uint32_t pc, uint8_t outcome)
{

    switch (bpType)
    {
    case STATIC:
        break;
    case GSHARE:
        return train_gshare(pc, outcome);
    case TOURNAMENT:
        return train_tournament(pc, outcome);
    case CUSTOM:
        return train_custom(pc, outcome);
    default:
        break;
    }
}
