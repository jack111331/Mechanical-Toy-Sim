#include <iostream>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <conio.h>
#include <thread>
#include <mutex>
#include <ctime>
#include <bitset>

#ifdef _WIN32
#include <windows.h>
#elif __unix__
#include <unistd.h>
#endif

#define DEBUG_FLAG
#define TOGGLE_SOLUTION

// Colorful output define
#define WHITE_COLOR "\033[97m"
#define RED_COLOR "\033[31m"
#define GREEN_COLOR "\033[32m"
#define BLUE_COLOR "\033[34m"
#define YELLOW_COLOR "\033[33m"
#define DEFAULT_COLOR "\033[0m"


// Parameter
#define INVALID_BAR_INDEX -1
#define INFINITY_COHERENCE 1e6
#define MAXIMUM_BAR 16
#define MAXIMUM_TRACK_POS 2*MAXIMUM_BAR
#define MAXIMUM_INTERLEAVE_TRACK_POS 2*MAXIMUM_TRACK_POS
#define SLIDING_RAIL_AMOUNT 4
#define TRACK_AMOUNT 5
#define MAXIMUM_PHYSICAL_OFFSET_ON_RAIL 10.0f
#define UNIT_PHYSICAL_OFFSET (MAXIMUM_PHYSICAL_OFFSET_ON_RAIL / MAXIMUM_BAR)
#define RAIL_BAR_GENERATE_PROBABILITY 0.5f
#define CHECKPOINT_GENERATE_PROBABILITY 0.3f

// Event System
#define TIME_TRIGGER_EVENT
#ifdef TIME_TRIGGER_EVENT
  // unit (second)
  #define TIME_TRIGGER_INTERVAL 120
#endif
// #define CHECKPOINT_TRIGGER_EVENT
// #ifdef CHECKPOINT_TRIGGER_EVENT
//   #define TRIGGER_CHECKPOINT_PROBABILITY 0.1f
// #endif


// Rail bar layout
// #define ORDERED_BAR_LAYOUT // proposed by Edge, Deprecated
#define INTERLEAVED_BAR_LAYOUT // proposed by Jia-wei

// Checkpoint generation
#define ONE_SET_CHECKPOINT_GENERATION
#ifdef ONE_SET_CHECKPOINT_GENERATION
  #define CHECKPOINT_AMOUNT 4
#endif
// #define MULTIPLE_SET_CHECKPOINT_GENERATION

// Progress bar generation
#ifdef ONE_SET_CHECKPOINT_GENERATION
  //#define CHECKPOINT_TO_CHECKPOINT_DISTANCE // proposed by vbscript Yu-De
  #define CHECKPOINT_TO_ENDTRACK_DISTANCE // proposed by Jia-wei
#endif
// #ifdef MULTIPLE_SET_CHECKPOINT_GENERATION
//   #define TO_MAXIMUM_CHECKPOINT_DISTANCE // proposed by Edge
//   #ifdef TO_MAXIMUM_CHECKPOINT_DISTANCE
//     #define MAXIMUM_FULFILL_CHECKPOINT 6
//   #endif
// #endif

// GUI related
#define TRANSPOSE_GUI


// Functional macro
// Generate 0000100000000000, 0100000000000000 and so on
#define MASK_AT(i) (1ULL << (i))
#define LOWEST_BIT(n) ((n)&(-n))
#define IS_RIGHT_FIRST(n) (((n)&0xaaaaaaaa)?true:false)
#define ALL_LOWER_BIT(n) ((n) | ((n)-1))
#define MASK_OUT_ALL_LOWER_BIT(n, b) ((n) & (~ALL_LOWER_BIT(b)))
#define RIGHT_RAIL(i) (i)
#define LEFT_RAIL(i) (i-1)

typedef struct RailStatus {
    uint16_t m_activeBar;

    size_t m_offset;

    size_t m_adviceCompleteOffset; // Pre-compute advice rail offset

    RailStatus() : m_activeBar(0), m_offset(0) { }

} RailStatus;

typedef struct TrackStatus {
    uint64_t m_checkpoint;

#ifdef CHECKPOINT_TRIGGER_EVENT
    uint64_t m_triggerCheckpoint;
#endif

    TrackStatus() : m_checkpoint(0) { }

} TrackStatus;

typedef struct GameStatus {
    RailStatus *m_railStatusList;

    TrackStatus *m_trackStatusList;

    size_t m_startTrack;

    size_t m_endTrack;

    int m_adviceAnswerTrackPath[64];

    int m_achievedCheckpoint;

#ifdef CHECKPOINT_TRIGGER_EVENT
    bool m_isTriggerCheckpoint;
#endif

    bool m_isComplete;

    float m_progressBar;

    // For debug purpose
    float m_debug;
    int m_currentEndTrack;
    int m_currentTrackHistory[64];

} GameStatus;

typedef struct InputStatus {
    float m_physicalOffset[SLIDING_RAIL_AMOUNT];

    InputStatus() : m_physicalOffset{} { }

} InputStatus;

typedef struct GraphProperty {
    enum struct GraphIcon {
        SPACE,
        BAR,
        DASH
    };
    GraphIcon m_content;

    enum struct GraphColor {
        WHITE,
        RED,
        GREEN,
        BLUE,
        YELLOW,
        DEFAULT
    };

    GraphColor m_color;

    GraphProperty() : m_content(GraphIcon::SPACE), m_color(GraphColor::WHITE) { }

    GraphProperty (GraphIcon icon, GraphColor color) : m_content(icon), m_color(color) { }

    static void printColor(GraphColor color) {
        if (color == GraphColor::WHITE) {
            printf(WHITE_COLOR);
        } else if (color == GraphColor::RED) {
            printf(RED_COLOR);
        } else if (color == GraphColor::GREEN) {
            printf(GREEN_COLOR);
        } else if (color == GraphColor::BLUE) {
            printf(BLUE_COLOR);
        } else if (color == GraphColor::YELLOW) {
            printf(YELLOW_COLOR);
        } else if (color == GraphColor::DEFAULT) {
            printf(DEFAULT_COLOR);
        }
    }

    void print (bool isTranspose) {
        printColor(m_color);
        if(m_content == GraphIcon::SPACE) {
            printf(" ");
            return;
        }
        if(m_content == GraphIcon::BAR) {
            printf(isTranspose?"-":"|");
        } else {
            printf(isTranspose?"|":"-");
        }
        printColor(GraphColor::DEFAULT);
    }

} GraphProperty;

// utility
static float seed = time(nullptr);

float randomFloat () {
    seed -= rand();
    return fabs(modf(sin(seed * 12.9898) * 43758.5453, nullptr));
}

float clamp (float value, float min, float max) {
    return value < min ? min : ((value > max) ? max : value);
}

time_t getCurrentTimeInSecond () {
    time_t rawtime;
    time(&rawtime);
    return rawtime;
}

double getDiffSecond (time_t begin, time_t end) {
  return difftime(end, begin);
}

bool isRailInBoundary (int currentRail) {
    return currentRail >= 0 ? (currentRail < SLIDING_RAIL_AMOUNT? true: false): false;
}

float coherenceBetweenBar (const RailStatus *rail) {
   float cumulatedCoherence = .0f;
   int prevActiveBar = INVALID_BAR_INDEX;
   for (int i = 0; i < MAXIMUM_BAR; ++i) {
      if (rail->m_activeBar & MASK_AT(i)) {
          // Calculate current active bar and previous detected active bar's distance, get its reciprocal
          if (prevActiveBar != INVALID_BAR_INDEX) {
              cumulatedCoherence += 1.0f / static_cast<float>(i - prevActiveBar);
          }
          prevActiveBar = i;
      }
   }
   return prevActiveBar==INVALID_BAR_INDEX ? INFINITY_COHERENCE : cumulatedCoherence;
}

// Input update
void railInputMapping (float physicalOffsetOnRailTrack, RailStatus *rail) {
    // Hardware dependent
    rail->m_offset = physicalOffsetOnRailTrack / UNIT_PHYSICAL_OFFSET;
}

void railInputUpdate (const InputStatus *inputStatus, GameStatus *game) {
    for (int i = 0;i < SLIDING_RAIL_AMOUNT; ++i) {
        railInputMapping(inputStatus->m_physicalOffset[i], &game->m_railStatusList[i]);
    }
}

// Game related
const float MAXIMUM_COHERENCE = 3.693f;

RailStatus *generateGhostLegGraph () {
    // DYNAMIC: Declare rail status memory space and initialize it
    RailStatus *railList = new RailStatus[SLIDING_RAIL_AMOUNT]();
    int regenerateTime = 0;
    for (int i = 0; i < SLIDING_RAIL_AMOUNT; ++i) {
        // Ensure coherence between bars of current rail is under specified MAXIMUM_COHERENCE
        do {
            railList[i].m_activeBar = 0;
            for (int j = 0; j < MAXIMUM_BAR; ++j) {
                if (randomFloat() <= RAIL_BAR_GENERATE_PROBABILITY) {
                    railList[i].m_activeBar = railList[i].m_activeBar | MASK_AT(j);
                }
            }
            regenerateTime++;
            if (regenerateTime > 10000) {
                printf("The graph is too hard to generate, please either change\nyour coherence upper bound or change your rail bar generate probability.\n");
                exit(1);
            }
        } while (coherenceBetweenBar(&railList[i]) >= MAXIMUM_COHERENCE);
    }
    return railList;
}

#ifdef ONE_SET_CHECKPOINT_GENERATION
bool isAdviceOffsetReachEnd (const GameStatus *game) {
    int globalTrackTestPos = 0;
    int currentTrack = game->m_startTrack; // 0 ~ TRACK_AMOUNT-1
    while (globalTrackTestPos < MAXIMUM_INTERLEAVE_TRACK_POS) {
        int switchToSide = 0; // 1 is right, -1 is left, 0 dont' move to either side

        if (isRailInBoundary(RIGHT_RAIL(currentTrack)) && globalTrackTestPos/2 >= game->m_railStatusList[RIGHT_RAIL(currentTrack)].m_adviceCompleteOffset && globalTrackTestPos/2 < game->m_railStatusList[RIGHT_RAIL(currentTrack)].m_adviceCompleteOffset + MAXIMUM_BAR) {
            // Mapping global test pos to current rail's right track local test pos
            int localTrackTestPos = globalTrackTestPos/2 - game->m_railStatusList[RIGHT_RAIL(currentTrack)].m_adviceCompleteOffset;
            if (game->m_railStatusList[RIGHT_RAIL(currentTrack)].m_activeBar & MASK_AT(localTrackTestPos)) {
                if (((currentTrack & 1) ^ (globalTrackTestPos & 1)) == 1) {
                    switchToSide = 1;
                }
            }
        }
        if (isRailInBoundary(LEFT_RAIL(currentTrack)) && globalTrackTestPos/2 >= game->m_railStatusList[LEFT_RAIL(currentTrack)].m_adviceCompleteOffset && globalTrackTestPos/2 < game->m_railStatusList[LEFT_RAIL(currentTrack)].m_adviceCompleteOffset + MAXIMUM_BAR) {
            // Mapping global test pos to current rail's left track local test pos
            int localTrackTestPos = globalTrackTestPos/2 - game->m_railStatusList[LEFT_RAIL(currentTrack)].m_adviceCompleteOffset;
            if (game->m_railStatusList[LEFT_RAIL(currentTrack)].m_activeBar & MASK_AT(localTrackTestPos)) {
                if (((currentTrack ^ globalTrackTestPos) & 1) == 0) {
                    switchToSide = -1;
                }
            }
        }
        // switch current test track to left or right side or don't switch
        currentTrack += switchToSide;
        globalTrackTestPos++;
    }

    return currentTrack == game->m_endTrack;
}

void updateAdviceAnswerTrackPath (GameStatus *game) {
    int globalTrackTestPos = 0;
    int currentTrack = game->m_startTrack; // 0 ~ TRACK_AMOUNT-1
    while (globalTrackTestPos < MAXIMUM_INTERLEAVE_TRACK_POS) {
        // update advice track
        game->m_adviceAnswerTrackPath[globalTrackTestPos] = currentTrack;
        int switchToSide = 0; // 1 is right, -1 is left, 0 dont' move to either side

        if (isRailInBoundary(RIGHT_RAIL(currentTrack)) && globalTrackTestPos/2 >= game->m_railStatusList[RIGHT_RAIL(currentTrack)].m_offset && globalTrackTestPos/2 < game->m_railStatusList[RIGHT_RAIL(currentTrack)].m_adviceCompleteOffset + MAXIMUM_BAR) {
            // Mapping global test pos to current rail's right track local test pos
            int localTrackTestPos = globalTrackTestPos/2 - game->m_railStatusList[RIGHT_RAIL(currentTrack)].m_adviceCompleteOffset;
            if (game->m_railStatusList[RIGHT_RAIL(currentTrack)].m_activeBar & MASK_AT(localTrackTestPos)) {
                if (((currentTrack & 1) ^ (globalTrackTestPos & 1)) == 1) {
                    switchToSide = 1;
                }
            }
        }
        if (isRailInBoundary(LEFT_RAIL(currentTrack)) && globalTrackTestPos/2 >= game->m_railStatusList[LEFT_RAIL(currentTrack)].m_offset && globalTrackTestPos/2 < game->m_railStatusList[LEFT_RAIL(currentTrack)].m_adviceCompleteOffset + MAXIMUM_BAR) {
            // Mapping global test pos to current rail's left track local test pos
            int localTrackTestPos = globalTrackTestPos/2 - game->m_railStatusList[LEFT_RAIL(currentTrack)].m_adviceCompleteOffset;
            if (game->m_railStatusList[LEFT_RAIL(currentTrack)].m_activeBar & MASK_AT(localTrackTestPos)) {
                if (((currentTrack & 1) ^ (globalTrackTestPos & 1)) == 0) {
                    switchToSide = -1;
                }
            }
        }
        // switch current test track to left or right side or don't switch
        currentTrack += switchToSide;
        globalTrackTestPos++;
    }
}

void findAdmissibleAnswer (GameStatus *game) {
    // this will update game->m_railStatusList->m_adviceCompleteOffset and game->m_adviceAnswerTrackPath
    do {
        for (int i = 0; i < SLIDING_RAIL_AMOUNT; ++i) {
            game->m_railStatusList[i].m_adviceCompleteOffset = (uint32_t)(randomFloat() * MAXIMUM_BAR);
        }
    } while (!isAdviceOffsetReachEnd(game));
    updateAdviceAnswerTrackPath(game);
}

void useAdmissibleAnswerToTagCheckpoint(GameStatus *game) {
    int globalTrackTestPos = 0;
    int currentTrack = game->m_startTrack; // 0 ~ TRACK_AMOUNT-1
    // checkpointAmount + 1, plus one is to prevent from choosing checkpoint at end point
    int checkpointSpacing = (MAXIMUM_INTERLEAVE_TRACK_POS - 1) / CHECKPOINT_AMOUNT;
    while (globalTrackTestPos < MAXIMUM_INTERLEAVE_TRACK_POS) {
        int switchToSide = 0; // 1 is right, -1 is left, 0 dont' move to either side

        if (isRailInBoundary(RIGHT_RAIL(currentTrack)) && globalTrackTestPos/2 >= game->m_railStatusList[RIGHT_RAIL(currentTrack)].m_adviceCompleteOffset && globalTrackTestPos/2 < game->m_railStatusList[RIGHT_RAIL(currentTrack)].m_adviceCompleteOffset + MAXIMUM_BAR) {
            // Mapping global test pos to current rail's right track local test pos
            int localTrackTestPos = globalTrackTestPos/2 - game->m_railStatusList[RIGHT_RAIL(currentTrack)].m_adviceCompleteOffset;
            if (game->m_railStatusList[RIGHT_RAIL(currentTrack)].m_activeBar & MASK_AT(localTrackTestPos)) {
                if (((currentTrack & 1) ^ (globalTrackTestPos & 1)) == 1) {
                    switchToSide = 1;
                }
            }
        }
        if (isRailInBoundary(LEFT_RAIL(currentTrack)) && globalTrackTestPos/2 >= game->m_railStatusList[LEFT_RAIL(currentTrack)].m_adviceCompleteOffset && globalTrackTestPos/2 < game->m_railStatusList[LEFT_RAIL(currentTrack)].m_adviceCompleteOffset + MAXIMUM_BAR) {
            // Mapping global test pos to current rail's left track local test pos
            int localTrackTestPos = globalTrackTestPos/2 - game->m_railStatusList[LEFT_RAIL(currentTrack)].m_adviceCompleteOffset;
            if (game->m_railStatusList[LEFT_RAIL(currentTrack)].m_activeBar & MASK_AT(localTrackTestPos)) {
                if (((currentTrack & 1) ^ (globalTrackTestPos & 1)) == 0) {
                    switchToSide = -1;
                }
            }
        }
        // switch current test track to left or right side or don't switch
        currentTrack += switchToSide;
        globalTrackTestPos++;
        // Generate checkpoint
        if (globalTrackTestPos % checkpointSpacing == 0) {
            game->m_trackStatusList[currentTrack].m_checkpoint = (game->m_trackStatusList[currentTrack].m_checkpoint | MASK_AT(globalTrackTestPos));
        }
    }
}

void updateCheckpoint (TrackStatus *trackList, GameStatus *game) {
    for (int i = 0; i < TRACK_AMOUNT; ++i) {
        trackList[i].m_checkpoint = 0;
    }
    findAdmissibleAnswer(game);
    useAdmissibleAnswerToTagCheckpoint(game);
}
#endif
#ifdef MULTIPLE_SET_CHECKPOINT_GENERATION
void updateCheckpoint (TrackStatus *trackList, GameStatus *game) {
    for (int i = 0; i < TRACK_AMOUNT; ++i) {
        trackList[i].m_checkpoint = 0;
        for (int j = 0; j < MAXIMUM_INTERLEAVE_TRACK_POS; ++j) {
            if (randomFloat() <= CHECKPOINT_GENERATE_PROBABILITY) {
                trackList[i].m_checkpoint = trackList[i].m_checkpoint | MASK_AT(j);
            }
        }
    }
}
bool isAnswerExist () {
    // TODO
}
#endif

TrackStatus *generateCheckpoint (GameStatus *game) {
    // DYNAMIC: Declare rail status memory space and initialize it
    TrackStatus *trackList = new TrackStatus[TRACK_AMOUNT]();
    game->m_trackStatusList = trackList;
    updateCheckpoint(trackList, game);
    // TODO need sanity check to ensure there is always at least one answer exist in MULTIPLE_SET_CHECKPOINT_GENERATION situation

    return trackList;
}

#ifdef CHECKPOINT_TRIGGER_EVENT
void updateTriggerCheckpoint (GameStatus *game, float triggerCheckpointGenProb) {
    for (int i = 0; i < TRACK_AMOUNT; ++i) {
        game->m_trackStatusList[i].m_triggerCheckpoint = 0;
        for (int j = 0; j < MAXIMUM_INTERLEAVE_TRACK_POS; ++j) {
            if ((game->m_trackStatusList[i].m_checkpoint & MASK_AT(j)) && randomFloat() <= triggerCheckpointGenProb) {
                game->m_trackStatusList[i].m_triggerCheckpoint = game->m_trackStatusList[i].m_triggerCheckpoint | MASK_AT(j);
            }
        }
    }
}
#endif

GameStatus *generateGame() {
    GameStatus *game = new GameStatus();
    game->m_startTrack = floor(randomFloat() * SLIDING_RAIL_AMOUNT);
    game->m_endTrack = floor(randomFloat() * SLIDING_RAIL_AMOUNT);
    game->m_railStatusList = generateGhostLegGraph();
    game->m_trackStatusList = generateCheckpoint(game);
#ifdef CHECKPOINT_TRIGGER_EVENT
    updateTriggerCheckpoint(game, TRIGGER_CHECKPOINT_PROBABILITY);
#endif
    game->m_progressBar = .0f;
    return game;
}

float progressBarUpdate(GameStatus *game) {

#ifdef ONE_SET_CHECKPOINT_GENERATION
    int globalTrackTestPos = 0;
    int currentTrack = game->m_startTrack; // 0 ~ TRACK_AMOUNT-1
    int checkpointSpacing = (MAXIMUM_INTERLEAVE_TRACK_POS - 1) / CHECKPOINT_AMOUNT;
    int currentAchievedCheckpointTrack;
    int currentAchievedCheckpointPos;
    int latestAchievedCheckpoint = 0;
    while (globalTrackTestPos < MAXIMUM_INTERLEAVE_TRACK_POS) {
        if ((latestAchievedCheckpoint + 1) * checkpointSpacing == globalTrackTestPos) {
            if ((game->m_trackStatusList[currentTrack].m_checkpoint & MASK_AT(globalTrackTestPos))) {
                currentAchievedCheckpointTrack = currentTrack;
                currentAchievedCheckpointPos = globalTrackTestPos;
                latestAchievedCheckpoint++;
            } else {
                break;
            }
        }

        int switchToSide = 0; // 1 is right, -1 is left, 0 dont' move to either side
        if (isRailInBoundary(RIGHT_RAIL(currentTrack)) && globalTrackTestPos/2 >= game->m_railStatusList[RIGHT_RAIL(currentTrack)].m_offset && globalTrackTestPos/2 < game->m_railStatusList[RIGHT_RAIL(currentTrack)].m_offset + MAXIMUM_BAR) {
            // Mapping global test pos to current rail's right track local test pos
            int localTrackTestPos = globalTrackTestPos/2 - game->m_railStatusList[RIGHT_RAIL(currentTrack)].m_offset;
            if (game->m_railStatusList[RIGHT_RAIL(currentTrack)].m_activeBar & MASK_AT(localTrackTestPos)) {
                if (((currentTrack & 1) ^ (globalTrackTestPos & 1)) == 1) {
                    switchToSide = 1;
                }
            }
        }
        if (isRailInBoundary(LEFT_RAIL(currentTrack)) && globalTrackTestPos/2 >= game->m_railStatusList[LEFT_RAIL(currentTrack)].m_offset && globalTrackTestPos/2 < game->m_railStatusList[LEFT_RAIL(currentTrack)].m_offset + MAXIMUM_BAR) {
            // Mapping global test pos to current rail's left track local test pos
            int localTrackTestPos = globalTrackTestPos/2 - game->m_railStatusList[LEFT_RAIL(currentTrack)].m_offset;
            if (game->m_railStatusList[LEFT_RAIL(currentTrack)].m_activeBar & MASK_AT(localTrackTestPos)) {
                if (((currentTrack & 1) ^ (globalTrackTestPos & 1)) == 0) {
                    switchToSide = -1;
                }
            }
        }
        // switch current test track to left or right side or don't switch
        currentTrack += switchToSide;
        globalTrackTestPos++;
    }
  #ifdef CHECKPOINT_TO_CHECKPOINT_DISTANCE
    // TODO still thinking about how to get the path distance to next checkpoint 0.0
    if (currentAchievedCheckpointTrack >= checkpointSpacing * CHECKPOINT_AMOUNT) {
        // Achieved last checkpoint and is seeking for the end point
        int verticalLength = MAXIMUM_INTERLEAVE_TRACK_POS - currentAchievedCheckpointPos;
        // DISCUSS: Should we amplify the impact of horizontalLength(?
        int horizontalLength = game->m_endTrack - currentAchievedCheckpointTrack;
        game->m_progressBar = sqrt(verticalLength * verticalLength + horizontalLength * horizontalLength);
    } else {
        // // Didn't achieve last checkpoint
        // int verticalLength = 32 - currentAchievedCheckpointPos;
        // // DISCUSS: Should we amplify the impact of horizontalLength(?
        // int horizontalLength = m_adviceAnswerTrackPath[currentAchievedCheckpointTrack] - currentAchievedCheckpointTrack;
        // game->m_progressBar = sqrt(verticalLength * verticalLength + horizontalLength * horizontalLength);
    }
  #endif
  #ifdef CHECKPOINT_TO_ENDTRACK_DISTANCE
    globalTrackTestPos = latestAchievedCheckpoint * checkpointSpacing;
    while (globalTrackTestPos < MAXIMUM_INTERLEAVE_TRACK_POS) {
        if (currentTrack == game->m_adviceAnswerTrackPath[globalTrackTestPos] || globalTrackTestPos >= (latestAchievedCheckpoint + 1) * checkpointSpacing) {
            break;
        }

        int switchToSide = 0; // 1 is right, -1 is left, 0 dont' move to either side

        if (isRailInBoundary(RIGHT_RAIL(currentTrack)) && globalTrackTestPos/2 >= game->m_railStatusList[RIGHT_RAIL(currentTrack)].m_offset && globalTrackTestPos/2 < game->m_railStatusList[RIGHT_RAIL(currentTrack)].m_offset + MAXIMUM_BAR) {
            // Mapping global test pos to current rail's right track local test pos
            int localTrackTestPos = globalTrackTestPos/2 - game->m_railStatusList[RIGHT_RAIL(currentTrack)].m_offset;
            if (game->m_railStatusList[RIGHT_RAIL(currentTrack)].m_activeBar & MASK_AT(localTrackTestPos)) {
                if (((currentTrack & 1) ^ (globalTrackTestPos & 1)) == 1) {
                    switchToSide = 1;
                }
            }
        }
        if (isRailInBoundary(LEFT_RAIL(currentTrack)) && globalTrackTestPos/2 >= game->m_railStatusList[LEFT_RAIL(currentTrack)].m_offset && globalTrackTestPos/2 < game->m_railStatusList[LEFT_RAIL(currentTrack)].m_offset + MAXIMUM_BAR) {
            // Mapping global test pos to current rail's left track local test pos
            int localTrackTestPos = globalTrackTestPos/2 - game->m_railStatusList[LEFT_RAIL(currentTrack)].m_offset;
            if (game->m_railStatusList[LEFT_RAIL(currentTrack)].m_activeBar & MASK_AT(localTrackTestPos)) {
                if (((currentTrack & 1) ^ (globalTrackTestPos & 1)) == 0) {
                    switchToSide = -1;
                }
            }
        }
        // switch current test track to left or right side or don't switch
        currentTrack += switchToSide;
        globalTrackTestPos++;
    }
    if (latestAchievedCheckpoint >= CHECKPOINT_AMOUNT) {
        // Issue: ProgressBar update
        // Achieved last checkpoint and is seeking for the end point
        // The tri-operator is for detecting if there is no contact with right end track
        int toEndTrackLength = (MAXIMUM_INTERLEAVE_TRACK_POS - globalTrackTestPos) <= 0 ? checkpointSpacing : (MAXIMUM_INTERLEAVE_TRACK_POS - globalTrackTestPos);
        game->m_progressBar = 1.0f - (static_cast<float>(toEndTrackLength) / static_cast<float>(checkpointSpacing));
    } else {
        // Didn't achieve last checkpoint
        int toEndTrackLength = (latestAchievedCheckpoint + 1) * checkpointSpacing - globalTrackTestPos;
        // if the globalTrackTestPos isn't even touch the next checkpoint, then toEndTrackLength will appear to be 0
        if (toEndTrackLength == 0) {
            game->m_progressBar = 0.0f;
        } else {
            game->m_progressBar = 1.0f - (static_cast<float>(toEndTrackLength) / static_cast<float>(checkpointSpacing));
        }
    }
    game->m_debug = globalTrackTestPos;
  #endif
#endif

#ifdef MULTIPLE_SET_CHECKPOINT_GENERATION
  #ifdef TO_MAXIMUM_CHECKPOINT_DISTANCE
      game->m_progressBar = clamp(static_cast<float>(game->m_achievedCheckpoint / MAXIMUM_FULFILL_CHECKPOINT), .0f, 1.0f);
  #endif
#endif
}

void updateGameInfo(GameStatus *game) {
    // Should use 64 position to accurately present its behavior
    int globalTrackTestPos = 0;
    int currentTrack = game->m_startTrack; // 0 ~ TRACK_AMOUNT-1
    game->m_achievedCheckpoint = 0;
#ifdef CHECKPOINT_TRIGGER_EVENT
    game->m_isTriggerCheckpoint = false;
#endif
    while (globalTrackTestPos < MAXIMUM_INTERLEAVE_TRACK_POS) {
        // For debug purpose
        game->m_currentTrackHistory[globalTrackTestPos] = currentTrack;

        game->m_achievedCheckpoint += (game->m_trackStatusList[currentTrack].m_checkpoint & MASK_AT(globalTrackTestPos)) ? 1 : 0;

#ifdef CHECKPOINT_TRIGGER_EVENT
        game->m_isTriggerCheckpoint = std::max(game->m_isTriggerCheckpoint, (game->m_trackStatusList[currentTrack].m_triggerCheckpoint & MASK_AT(globalTrackTestPos)) ? true : false);
#endif

        int switchToSide = 0; // 1 is right, -1 is left, 0 dont' move to either side

        if (isRailInBoundary(RIGHT_RAIL(currentTrack)) && globalTrackTestPos/2 >= game->m_railStatusList[RIGHT_RAIL(currentTrack)].m_offset && globalTrackTestPos/2 < game->m_railStatusList[RIGHT_RAIL(currentTrack)].m_offset + MAXIMUM_BAR) {
            // Mapping global test pos to current rail's right track local test pos
            int localTrackTestPos = globalTrackTestPos/2 - game->m_railStatusList[RIGHT_RAIL(currentTrack)].m_offset;
            if (game->m_railStatusList[RIGHT_RAIL(currentTrack)].m_activeBar & MASK_AT(localTrackTestPos)) {
                if (((currentTrack & 1) ^ (globalTrackTestPos & 1)) == 1) {
                    switchToSide = 1;
                }
            }
        }
        if (isRailInBoundary(LEFT_RAIL(currentTrack)) && globalTrackTestPos/2 >= game->m_railStatusList[LEFT_RAIL(currentTrack)].m_offset && globalTrackTestPos/2 < game->m_railStatusList[LEFT_RAIL(currentTrack)].m_offset + MAXIMUM_BAR) {
            // Mapping global test pos to current rail's left track local test pos
            int localTrackTestPos = globalTrackTestPos/2 - game->m_railStatusList[LEFT_RAIL(currentTrack)].m_offset;
            if (game->m_railStatusList[LEFT_RAIL(currentTrack)].m_activeBar & MASK_AT(localTrackTestPos)) {
                if (((currentTrack ^ globalTrackTestPos) & 1) == 0) {
                    switchToSide = -1;
                }
            }
        }
        // switch current test track to left or right side or don't switch
        currentTrack += switchToSide;
        globalTrackTestPos++;
    }
    game->m_currentEndTrack = currentTrack;
    game->m_isComplete = (game->m_endTrack == currentTrack) && (game->m_achievedCheckpoint >= CHECKPOINT_AMOUNT);

    progressBarUpdate(game);
}

void eventDrivenCheckpointUpdate(int currentTime, GameStatus *game) {
#ifdef TIME_TRIGGER_EVENT
    // NOTICE: Make sure to invoke this routine every second
    if (currentTime != 0 && currentTime % TIME_TRIGGER_INTERVAL == 0) {
        updateCheckpoint(game->m_trackStatusList, game);
    }
#endif

#ifdef CHECKPOINT_TRIGGER_EVENT
    // NOTICE: should called after updateGameInfo()
    if (game->m_isTriggerCheckpoint) {
        updateCheckpoint(game->m_trackStatusList, game);
    }
#endif
}

GraphProperty drawInterleavedTrackAt(const GameStatus *game, int trackId, int trackPos) {
    // With color ver.
    #ifdef TOGGLE_SOLUTION
    if (game->m_adviceAnswerTrackPath[trackPos] == trackId) {
        if (game->m_trackStatusList[trackId].m_checkpoint & MASK_AT(trackPos)) {
            return GraphProperty(GraphProperty::GraphIcon::BAR, GraphProperty::GraphColor::YELLOW);
        } else {
            return GraphProperty(GraphProperty::GraphIcon::BAR, GraphProperty::GraphColor::BLUE);
        }
    } else
    #endif
    #ifdef CHECKPOINT_TRIGGER_EVENT
    if (game->m_trackStatusList[trackId].m_triggerCheckpoint & MASK_AT(trackPos)) {
        return GraphProperty(GraphProperty::GraphIcon::BAR, GraphProperty::GraphColor::RED);
    } else
    #endif
    if (game->m_trackStatusList[trackId].m_checkpoint & MASK_AT(trackPos)) {
        return GraphProperty(GraphProperty::GraphIcon::BAR, GraphProperty::GraphColor::GREEN);
    } else {
        return GraphProperty(GraphProperty::GraphIcon::BAR, GraphProperty::GraphColor::WHITE);
    }
    // Without color ver.
    // return GraphProperty(GraphProperty::GraphIcon::BAR, GraphProperty::GraphColor::WHITE);
}

GraphProperty drawInterleavedRailAt(const GameStatus *game, int railId, int trackPos) {
    // If trackPos is not on interleaved possible bar area, then it must print space
    if (((railId&1) ^ (trackPos&1)) == 0) {
        return GraphProperty(GraphProperty::GraphIcon::SPACE, GraphProperty::GraphColor::WHITE);
    }
    int barPhysicalPos = (trackPos - 2 * game->m_railStatusList[railId].m_offset);
    if (barPhysicalPos >= 0) {
        // prevent barPhysicalPos=-1 from divide it with 2 and get zero
        int barLogicalPos = barPhysicalPos / 2;
        if (game->m_railStatusList[railId].m_activeBar & MASK_AT(barLogicalPos)) {
            return GraphProperty(GraphProperty::GraphIcon::DASH, GraphProperty::GraphColor::WHITE);
        } else {
            return GraphProperty(GraphProperty::GraphIcon::SPACE, GraphProperty::GraphColor::WHITE);
        }
    } else {
        return GraphProperty(GraphProperty::GraphIcon::SPACE, GraphProperty::GraphColor::WHITE);
    }
}

void printGraph (const GameStatus *game) {
    GraphProperty graph[SLIDING_RAIL_AMOUNT + TRACK_AMOUNT][2 * MAXIMUM_TRACK_POS];
    for (int j = 0; j < MAXIMUM_INTERLEAVE_TRACK_POS; ++j) {
        for (int i = 0; i < SLIDING_RAIL_AMOUNT; ++i) {
            graph[i * 2][j] = drawInterleavedTrackAt(game, i, j);
            graph[i * 2 + 1][j] = drawInterleavedRailAt(game, i, j);
        }
        graph[SLIDING_RAIL_AMOUNT+TRACK_AMOUNT-1][j] = drawInterleavedTrackAt(game, TRACK_AMOUNT-1, j);
    }

#ifdef TRANSPOSE_GUI
    for (int i = 0; i < SLIDING_RAIL_AMOUNT + TRACK_AMOUNT; ++i) {
        for (int j = 0; j < MAXIMUM_INTERLEAVE_TRACK_POS; ++j) {
            graph[i][j].print(true);
        }
        puts("");
    }
#else
    for (int j = 0; j < MAXIMUM_INTERLEAVE_TRACK_POS; ++j) {
        for (int i = 0; i < SLIDING_RAIL_AMOUNT + TRACK_AMOUNT; ++i) {
            graph[i][j].print(false);
        }
        puts("");
    }
#endif
}

int main() {
    // Initialize
    GameStatus *game = generateGame();
    InputStatus *inputStatus = new InputStatus();
    updateGameInfo(game);
    while(game->m_isComplete) {
        delete game;
        game = generateGame();
    }

#ifdef DEBUG_FLAG
    printf("Initialized\n");
#endif

    printf("Usage:\n\nRight arrow(->) represent tuning next rail\n\nLeft arrow(<-) represent tuning previous rail\n\n         ^\nUp arrow(|) represent pull up current rail\n\nDown arrow(|) represent push down current rail\n           v\n\n");
    printf("\n\n\nPress r to refresh GUI\n\n\nPress Enter to start");
    getchar();

    bool dirtyFlag = true;

    int currentInputRail = 0;
    std::mutex dirtyMutex;

    std::thread ([&] () {
        // Input thread
        const float PHYSICAL_INVERVAL = 0.3f;
        while (true) {
            int ch;
            if ((ch = getch()) != 27) {
                if (ch == 0 || ch == 224) { // if the first value is esc
                    dirtyMutex.lock();
                    dirtyFlag = true;
                    switch (getch()) { // the real value
                        case 72:
                            // code for arrow up
                            if(inputStatus->m_physicalOffset[currentInputRail] + PHYSICAL_INVERVAL > MAXIMUM_PHYSICAL_OFFSET_ON_RAIL) {
                                inputStatus->m_physicalOffset[currentInputRail] = MAXIMUM_PHYSICAL_OFFSET_ON_RAIL;
                            } else {
                                inputStatus->m_physicalOffset[currentInputRail] = inputStatus->m_physicalOffset[currentInputRail] + PHYSICAL_INVERVAL;
                            }
                            railInputUpdate(inputStatus, game);
                            break;
                        case 80:
                            // code for arrow down
                            if(inputStatus->m_physicalOffset[currentInputRail] - PHYSICAL_INVERVAL < 0) {
                                inputStatus->m_physicalOffset[currentInputRail] = 0;
                            } else {
                                inputStatus->m_physicalOffset[currentInputRail] = inputStatus->m_physicalOffset[currentInputRail] - PHYSICAL_INVERVAL;
                            }
                            railInputUpdate(inputStatus, game);
                            break;
                        case 77:
                            // code for arrow right
                            currentInputRail = (currentInputRail+1)>=SLIDING_RAIL_AMOUNT?currentInputRail:currentInputRail+1;
                            break;
                        case 75:
                            // code for arrow left
                            currentInputRail = (currentInputRail-1)<0?currentInputRail:currentInputRail-1;
                            break;
                    }
                    dirtyMutex.unlock();
                }
            } else if(ch == 'r' || ch == 'R') {
                dirtyMutex.lock();
                dirtyFlag = true;
                dirtyMutex.unlock();
            }
        }
    }).detach();
    time_t startTime = getCurrentTimeInSecond();
    time_t currentTime = startTime;
    std::thread([&] () {
        // Time update loop thread
        while (true) {
#ifdef _WIN32
            Sleep(500);
#elif __unix__
            usleep(500000);
#endif
            currentTime = getCurrentTimeInSecond();
            // Toggle time update
            // dirtyMutex.lock();
            // dirtyFlag = true;
            // dirtyMutex.unlock();
            // printf("Update time\n");
        }
    }).detach();

    std::thread([&] () {
        // Game loop thread
        while (true) {
            updateGameInfo(game);
            eventDrivenCheckpointUpdate(getDiffSecond(startTime, currentTime), game);
        }
    }).detach();

    std::thread([&] () {
        // GUI thread
        while (!game->m_isComplete) {
            if (dirtyFlag) {
                // update GUI
                system("cls");
                printGraph(game);
#ifdef DEBUG_FLAG
                printf("\n\nCurrent time is %lf(s)\n", getDiffSecond(startTime, currentTime));
                printf("\n\nCurrent input rail: %d\n", currentInputRail);
                printf("\nPhysical offsets are (%f, %f, %f, %f)\n", inputStatus->m_physicalOffset[0], inputStatus->m_physicalOffset[1], inputStatus->m_physicalOffset[2], inputStatus->m_physicalOffset[3]);
                printf("\nLogical rail offsets are (%d, %d, %d, %d)\n", game->m_railStatusList[0].m_offset, game->m_railStatusList[1].m_offset, game->m_railStatusList[2].m_offset, game->m_railStatusList[3].m_offset);
                printf("\nRail bars:\n");
                for (int i = 0; i < SLIDING_RAIL_AMOUNT; ++i) {
                    std::cout << std::bitset<32>((uint32_t)(game->m_railStatusList[i].m_activeBar) << game->m_railStatusList[i].m_offset) << std::endl;
                }
                printf("\nTrack history:\n");
                for (int i = 0; i < MAXIMUM_INTERLEAVE_TRACK_POS; ++i) {
                    std::cout << game->m_currentTrackHistory[i] << " ";
                }
                printf("\nCheckpoint:\n");
                for (int i = 0; i < TRACK_AMOUNT; ++i) {
                    std::cout << std::bitset<64>(game->m_trackStatusList[i].m_checkpoint) << std::endl;
                }

                printf("\nGame start track is %d, end track is %d, current setting's end track is %d\n", game->m_startTrack, game->m_endTrack, game->m_currentEndTrack);
                printf("\nAchieved checkpoint amount is %d\n", game->m_achievedCheckpoint);
                printf("\nAdvice rail offsets are (%d, %d, %d, %d)\n", game->m_railStatusList[0].m_adviceCompleteOffset, game->m_railStatusList[1].m_adviceCompleteOffset, game->m_railStatusList[2].m_adviceCompleteOffset, game->m_railStatusList[3].m_adviceCompleteOffset);
                printf("\nProgress bar: %f\n", game->m_progressBar);
                printf("\nDebug: %f\n", game->m_debug);
#endif
                dirtyMutex.lock();
                dirtyFlag = false;
                dirtyMutex.unlock();
            }
        }
        std::cout << "Game Over" << std::endl;
    }).join();
}
