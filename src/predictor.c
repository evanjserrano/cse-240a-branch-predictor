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

#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)

//
// TODO:Student Information
//
const char* studentName = "Evan Serrano";
const char* studentID = "A15543204";
const char* email = "e1serran@ucsd.edu";

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
const int trn_pcBits = 10;        // PC register size (effective size for BP)
const int trn_ghrBits = 12;       // global history register size
const int trn_local_phtBits = 10; // local predictor pht entry size
const int trn_local_bhtBits = 3;  // local predictor bht entry size
const int trn_global_bhtBits = 2; // global preictor bht entry size
const int trn_chooserBits = 2;    // chooser entry size

const int trn_pcSize = 1 << trn_pcBits;
const int trn_ghrSize = 1 << trn_ghrBits;
const int trn_local_phtSize = 1 << trn_pcBits; // local predictor pht # entries
const int trn_local_bhtSize = 1 << trn_local_phtBits;
const int trn_global_bhtSize = 1 << trn_ghrBits;
const int trn_chooserSize = 1 << trn_ghrBits;

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

    memset(trn_local_bht, 3, trn_local_bhtSize);
    memset(trn_global_bht, 1, trn_local_bhtSize);
    memset(trn_chooser, 1, trn_local_bhtSize);
}

uint8_t tournament_predict(uint32_t pc)
{
    uint8_t prediction;

    // choose local or global
    uint16_t ghr = ghistory & (trn_ghrSize - 1);
    if ((trn_chooser[ghr] >> (trn_chooserBits - 1)) & 0x1) // msb of chooser
    {
        // global predictor (1)
        // get prediction based on ghr
        prediction = (trn_global_bht[ghr] >> (trn_global_bhtBits - 1)) & 0x1;
    }
    else
    {
        // local predictor (0)
        uint16_t pc_idx = pc & (trn_pcSize - 1);
        // get pattern of branch
        uint16_t pat = trn_local_pht[pc_idx] & (trn_local_phtSize - 1);
        // get prediction based on pattern
        prediction = (trn_local_bht[pat] >> (trn_local_bhtBits - 1)) & 0x1;
    }

    return prediction ? TAKEN : NOTTAKEN;
}

void train_tournament(uint32_t pc, uint8_t outcome)
{
    uint8_t global_pred;
    uint8_t local_pred;
    uint8_t choice;

    // choose local or global
    uint16_t ghr = ghistory & (trn_ghrSize - 1);
    choice = (trn_chooser[ghr] >> (trn_chooserBits - 1)) & 0x1;

    // global predictor (1)
    global_pred = (trn_global_bht[ghr] >> (trn_global_bhtBits - 1)) & 0x1;

    // local predictor (0)
    uint16_t pc_idx = pc & (trn_pcSize - 1);
    uint16_t pat = trn_local_pht[pc_idx] & (trn_local_phtSize - 1);
    local_pred = (trn_local_bht[pat] >> (trn_local_bhtBits - 1)) & 0x1;

    // update tables
    // only update choice if predictions differ
    if (global_pred != local_pred)
    {
        // global is correct
        if (outcome == global_pred)
        {
            trn_chooser[ghr] =
                MIN((trn_chooser[ghr] + 1), (1 << trn_chooserBits) - 1);
        }
        // local is correct
        else
        {
            trn_chooser[ghr] = MAX((trn_chooser[ghr] - 1), 0);
        }
    }

    // ghr and pattern table
    ghistory = ghistory << 1 | (outcome & 0x1);
    trn_local_pht[pc_idx] = trn_local_pht[pc_idx] << 1 | (outcome & 0x1);

    // branch history tables
    if (outcome == TAKEN)
    {
        trn_global_bht[ghr] =
            MIN((trn_global_bht[ghr] + 1), (1 << trn_global_bhtBits) - 1);
        trn_local_bht[ghr] =
            MIN((trn_local_bht[ghr] + 1), (1 << trn_local_bhtBits) - 1);
    }
    else
    {
        trn_global_bht[ghr] = MAX((trn_global_bht[ghr] - 1), 0);
        trn_local_bht[ghr] = MAX((trn_local_bht[ghr] - 1), 0);
    }
}

void cleanup_tournament()
{
    for (int i = 0; i < trn_local_bhtSize; i++)
    {
        uint8_t entry = trn_local_bht[i];
        printf("%03X: ", i);
        for (int j = 7; j >= 0; j--)
        {
            int bit = (entry >> j) & 0x1;
            printf("%i", bit);
        }
        printf("\n");
    }
    free(trn_local_pht);
    free(trn_local_bht);
    free(trn_global_bht);
    free(trn_chooser);
}

// custom functions
void init_custom()
{
}

uint8_t custom_predict(uint32_t pc)
{
    return NOTTAKEN;
}

void train_custom(uint32_t pc, uint8_t outcome)
{
}

void cleanup_custom()
{
}

void init_predictor()
{
    switch (bpType)
    {
    case STATIC:
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
