#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <mmsystem.h> 

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "gdi32.lib")

#define MAX_Q 30 
#define Q_PER_TIER 6 

// --- GAME STRUCTURES ---
struct Question {
    char text[256];
    char options[4][100];
    char correctAnswer;
    int prizeMoney;
};

struct Player {
    char name[100];
    int age;
    char location[100];
    char occupation[100];
};

int loadQuestions(const char* filename, struct Question questions[]);
void shuffleQuestions(struct Question array[], int n);
void printRules();
void playGame(struct Question gameDeck[], struct Question backupDeck[], struct Player p);

// --- GUI GLOBALS ---
HWND hwndMain;
HWND hQuestionLabel, hBtnA, hBtnB, hBtnC, hBtnD;
HWND hBtn50, hBtnFlip, hBtnDD, hBtnQuit;
HWND hEdit, hSubmit;
HANDLE hInputEvent;
char userAction = 0;

void gui_msgbox(const char* title, const char* msg) {
    char fullMsg[2048];
    snprintf(fullMsg, sizeof(fullMsg), "%s\n\n%s", title, msg);
    
    SetWindowLongA(hQuestionLabel, GWL_STYLE, WS_CHILD | WS_VISIBLE | SS_CENTER);
    SetWindowPos(hQuestionLabel, NULL, 10, 10, 560, 280, SWP_NOZORDER | SWP_FRAMECHANGED);
    SetWindowTextA(hQuestionLabel, fullMsg);
    
    ShowWindow(hBtnA, SW_HIDE); ShowWindow(hBtnB, SW_HIDE);
    ShowWindow(hBtnC, SW_HIDE); ShowWindow(hBtnD, SW_HIDE);
    ShowWindow(hBtn50, SW_HIDE); ShowWindow(hBtnFlip, SW_HIDE);
    ShowWindow(hBtnDD, SW_HIDE); ShowWindow(hBtnQuit, SW_HIDE);
    ShowWindow(hEdit, SW_HIDE);
    
    SetWindowTextA(hSubmit, "Continue");
    SetWindowPos(hSubmit, NULL, 240, 320, 120, 30, SWP_NOZORDER);
    ShowWindow(hSubmit, SW_SHOW);
    
    ResetEvent(hInputEvent);
    WaitForSingleObject(hInputEvent, INFINITE);
    
    SetWindowTextA(hSubmit, "Submit");
}

void gui_get_text(const char* prompt, char* buffer, int maxLen) {
    SetWindowLongA(hQuestionLabel, GWL_STYLE, WS_CHILD | WS_VISIBLE | SS_CENTER);
    SetWindowPos(hQuestionLabel, NULL, 100, 120, 400, 30, SWP_NOZORDER | SWP_FRAMECHANGED);
    SetWindowPos(hEdit, NULL, 150, 180, 300, 120, SWP_NOZORDER);
    SetWindowPos(hSubmit, NULL, 240, 320, 120, 30, SWP_NOZORDER);
    SetWindowTextA(hQuestionLabel, prompt);
    
    ShowWindow(hBtnA, SW_HIDE); ShowWindow(hBtnB, SW_HIDE);
    ShowWindow(hBtnC, SW_HIDE); ShowWindow(hBtnD, SW_HIDE);
    ShowWindow(hBtn50, SW_HIDE); ShowWindow(hBtnFlip, SW_HIDE);
    ShowWindow(hBtnDD, SW_HIDE); ShowWindow(hBtnQuit, SW_HIDE);
    
    ShowWindow(hEdit, SW_SHOW); ShowWindow(hSubmit, SW_SHOW);
    SetWindowTextA(hEdit, "");

    ResetEvent(hInputEvent);
    WaitForSingleObject(hInputEvent, INFINITE);
    
    GetWindowTextA(hEdit, buffer, maxLen);
}

char gui_ask_question(const char* fullText, const char* a, const char* b, const char* c, const char* d, int showLifelines) {
    SetWindowLongA(hQuestionLabel, GWL_STYLE, WS_CHILD | WS_VISIBLE | SS_CENTER);
    SetWindowPos(hQuestionLabel, NULL, 20, 20, 540, 150, SWP_NOZORDER | SWP_FRAMECHANGED);
    SetWindowTextA(hQuestionLabel, fullText);
    
    if (a && strlen(a) > 0) { SetWindowTextA(hBtnA, a); ShowWindow(hBtnA, SW_SHOW); } else ShowWindow(hBtnA, SW_HIDE);
    if (b && strlen(b) > 0) { SetWindowTextA(hBtnB, b); ShowWindow(hBtnB, SW_SHOW); } else ShowWindow(hBtnB, SW_HIDE);
    if (c && strlen(c) > 0) { SetWindowTextA(hBtnC, c); ShowWindow(hBtnC, SW_SHOW); } else ShowWindow(hBtnC, SW_HIDE);
    if (d && strlen(d) > 0) { SetWindowTextA(hBtnD, d); ShowWindow(hBtnD, SW_SHOW); } else ShowWindow(hBtnD, SW_HIDE);

    if (showLifelines) {
        ShowWindow(hBtn50, SW_SHOW); ShowWindow(hBtnFlip, SW_SHOW);
        ShowWindow(hBtnDD, SW_SHOW); ShowWindow(hBtnQuit, SW_SHOW);
    } else {
        ShowWindow(hBtn50, SW_HIDE); ShowWindow(hBtnFlip, SW_HIDE);
        ShowWindow(hBtnDD, SW_HIDE); ShowWindow(hBtnQuit, SW_HIDE);
    }
    ShowWindow(hEdit, SW_HIDE); ShowWindow(hSubmit, SW_HIDE);

    ResetEvent(hInputEvent);
    WaitForSingleObject(hInputEvent, INFINITE);
    return userAction;
}

BOOL CALLBACK SetFontCallback(HWND hwnd, LPARAM lParam) {
    SendMessage(hwnd, WM_SETFONT, (WPARAM)lParam, TRUE);
    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CREATE: {
            hQuestionLabel = CreateWindowA("STATIC", "", WS_CHILD | WS_VISIBLE | SS_CENTER, 20, 20, 540, 150, hwnd, NULL, NULL, NULL);
            hBtnA = CreateWindowA("BUTTON", "A", WS_CHILD | WS_VISIBLE | BS_MULTILINE, 20, 180, 260, 40, hwnd, (HMENU)10, NULL, NULL);
            hBtnB = CreateWindowA("BUTTON", "B", WS_CHILD | WS_VISIBLE | BS_MULTILINE, 300, 180, 260, 40, hwnd, (HMENU)11, NULL, NULL);
            hBtnC = CreateWindowA("BUTTON", "C", WS_CHILD | WS_VISIBLE | BS_MULTILINE, 20, 230, 260, 40, hwnd, (HMENU)12, NULL, NULL);
            hBtnD = CreateWindowA("BUTTON", "D", WS_CHILD | WS_VISIBLE | BS_MULTILINE, 300, 230, 260, 40, hwnd, (HMENU)13, NULL, NULL);
            
            hBtn50 = CreateWindowA("BUTTON", "50-50", WS_CHILD | WS_VISIBLE, 20, 290, 120, 30, hwnd, (HMENU)14, NULL, NULL);
            hBtnFlip = CreateWindowA("BUTTON", "Flip", WS_CHILD | WS_VISIBLE, 160, 290, 120, 30, hwnd, (HMENU)15, NULL, NULL);
            hBtnDD = CreateWindowA("BUTTON", "Double Dip", WS_CHILD | WS_VISIBLE, 300, 290, 120, 30, hwnd, (HMENU)16, NULL, NULL);
            hBtnQuit = CreateWindowA("BUTTON", "Quit", WS_CHILD | WS_VISIBLE, 440, 290, 120, 30, hwnd, (HMENU)17, NULL, NULL);
            
            hEdit = CreateWindowA("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 20, 340, 400, 30, hwnd, NULL, NULL, NULL);
            hSubmit = CreateWindowA("BUTTON", "Submit", WS_CHILD | WS_VISIBLE, 440, 340, 120, 30, hwnd, (HMENU)1, NULL, NULL);
            
            HFONT hFont = CreateFontA(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
            EnumChildWindows(hwnd, (WNDENUMPROC)SetFontCallback, (LPARAM)hFont);
            
            HFONT hQuestionFont = CreateFontA(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
            SendMessage(hQuestionLabel, WM_SETFONT, (WPARAM)hQuestionFont, TRUE);
            break;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == 1) { 
                userAction = 'S';
                SetEvent(hInputEvent);
            } else if (LOWORD(wParam) >= 10 && LOWORD(wParam) <= 17) {
                char actions[] = {'A', 'B', 'C', 'D', '1', '2', '3', 'Q'};
                userAction = actions[LOWORD(wParam) - 10];
                SetEvent(hInputEvent);
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            exit(0);
            break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

DWORD WINAPI GameThread(LPVOID lpParam) {
    srand((unsigned int)time(NULL));

    struct Question tier1[MAX_Q], tier2[MAX_Q], tier3[MAX_Q];
    struct Question gameDeck[15];
    struct Question backupDeck[3]; 
    struct Player p1; 

    // --- PLAYER PROFILE SETUP ---
    char msgBuf[512];
    gui_msgbox("Welcome", "WELCOME TO KAUN BANEGA CROREPATI!\nBefore we begin, let's get to know our contestant!");
    
    gui_get_text("Enter your Full Name:", p1.name, sizeof(p1.name));
    
    char ageStr[20];
    gui_get_text("Enter your Age:", ageStr, sizeof(ageStr));
    p1.age = atoi(ageStr);
    
    gui_get_text("Where are you from?:", p1.location, sizeof(p1.location));
    gui_get_text("What is your Occupation?:", p1.occupation, sizeof(p1.occupation));

    sprintf(msgBuf, "Fantastic! Welcome %s, a %d-year-old %s from %s!\nLet's see if you can win 1 Crore today.", p1.name, p1.age, p1.occupation, p1.location);
    gui_msgbox("Profile Complete", msgBuf);

    if (loadQuestions("questions1.txt", tier1) < Q_PER_TIER ||
        loadQuestions("questions2.txt", tier2) < Q_PER_TIER ||
        loadQuestions("questions3.txt", tier3) < Q_PER_TIER) {
        gui_msgbox("Error", "Error: Missing files or not enough questions. Run the generator first!");
        exit(1);
    }

    shuffleQuestions(tier1, MAX_Q);
    shuffleQuestions(tier2, MAX_Q);
    shuffleQuestions(tier3, MAX_Q);

    int kbcPrizes[15] = {1000, 2000, 3000, 5000, 10000, 
                         20000, 40000, 80000, 160000, 320000, 
                         640000, 1250000, 2500000, 5000000, 10000000};

    for (int i = 0; i < 5; i++) {
        gameDeck[i] = tier1[i];           
        gameDeck[i + 5] = tier2[i];       
        gameDeck[i + 10] = tier3[i];      
    }
    backupDeck[0] = tier1[5]; 
    backupDeck[1] = tier2[5]; 
    backupDeck[2] = tier3[5]; 

    for(int i = 0; i < 15; i++) {
        gameDeck[i].prizeMoney = kbcPrizes[i];
    }
    backupDeck[0].prizeMoney = kbcPrizes[0]; 
    backupDeck[1].prizeMoney = kbcPrizes[5]; 
    backupDeck[2].prizeMoney = kbcPrizes[10];

    printRules();
    
    playGame(gameDeck, backupDeck, p1);

    exit(0);
    return 0;
}

int main(int argc, char *argv[]) {
    HINSTANCE hInstance = GetModuleHandle(NULL);
    
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    wc.lpszClassName = "KBC_GUI";
    RegisterClassA(&wc);

    hwndMain = CreateWindowA("KBC_GUI", "Kaun Banega Crorepati", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 
                             CW_USEDEFAULT, CW_USEDEFAULT, 600, 450, NULL, NULL, hInstance, NULL);

    hInputEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    CreateThread(NULL, 0, GameThread, NULL, 0, NULL);
    
    MSG msg;
    while(GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

void playGame(struct Question gameDeck[], struct Question backupDeck[], struct Player p) {
    int currentPrize = 0;
    int safePrize = 0; 
    int used5050 = 0, usedFlip = 0, usedDoubleDip = 0;
    char answer;
    char msgBuf[512];

    for (int i = 0; i < 15; i++) {
        int answered = 0;
        int allowedAttempts = 1;
        int printQ = 1; 
        char fullQ[1024];

        while (!answered) {
            if (printQ) {
                if (i == 0) {
                    sprintf(fullQ, "Question for %s - Rs. %d:\n\n%s", p.name, gameDeck[i].prizeMoney, gameDeck[i].text);
                } else {
                    sprintf(fullQ, "Next Question for %s - Rs. %d:\n\n%s", p.name, gameDeck[i].prizeMoney, gameDeck[i].text);
                }
                printQ = 0; 
            }
            
            time_t start_time = time(NULL);
            answer = gui_ask_question(fullQ, gameDeck[i].options[0], gameDeck[i].options[1], gameDeck[i].options[2], gameDeck[i].options[3], 1);
            time_t end_time = time(NULL);
            
            if (difftime(end_time, start_time) > 45.0) {
                sprintf(msgBuf, "Oh no %s, you took %.0f seconds to answer. The limit is 45 seconds.\n>>> Game Over. You take home Rs. %d. <<<", p.name, difftime(end_time, start_time), safePrize);
                gui_msgbox("TIME UP!", msgBuf);
                PlaySound(TEXT("wrong.wav"), NULL, SND_FILENAME | SND_SYNC); 
                return;
            }

            if (answer == '1') {
                if (allowedAttempts == 2) gui_msgbox("Locked", "You are locked into Double Dip!");
                else if (used5050) gui_msgbox("Used", "You have already used your 50-50!");
                else {
                    used5050 = 1;
                    gui_msgbox("Lifeline", "--- 50-50 LIFELINE ACTIVATED ---");
                    int removed = 0;
                    for (int j = 0; j < 4 && removed < 2; j++) {
                        char optChar = 'A' + j;
                        if (optChar != gameDeck[i].correctAnswer) {
                            strcpy(gameDeck[i].options[j], ""); 
                            removed++;
                        }
                    }
                    printQ = 1; 
                }
            } 
            else if (answer == '2') {
                if (allowedAttempts == 2) gui_msgbox("Locked", "You are locked into Double Dip!");
                else if (usedFlip) gui_msgbox("Used", "You have already used Flip!");
                else {
                    usedFlip = 1;
                    int tierIndex = i / 5; 
                    int preservedPrize = gameDeck[i].prizeMoney; 
                    gameDeck[i] = backupDeck[tierIndex]; 
                    gameDeck[i].prizeMoney = preservedPrize; 
                    gui_msgbox("Lifeline", "--- QUESTION FLIPPED! ---");
                    printQ = 1; 
                }
            }
            else if (answer == '3') {
                if (usedDoubleDip) gui_msgbox("Used", "You have already used Double Dip!");
                else {
                    usedDoubleDip = 1;
                    allowedAttempts = 2; 
                    gui_msgbox("Lifeline", "--- DOUBLE DIP ACTIVATED ---\nYou have 2 attempts! Cannot quit or use other lifelines now.");
                }
            }
            else if (answer == 'Q') {
                if (allowedAttempts == 2) gui_msgbox("Locked", "Cannot quit during Double Dip!");
                else {
                    sprintf(msgBuf, ">>> %s chose to quit! You take home Rs. %d. Well played! <<<", p.name, currentPrize);
                    gui_msgbox("Quit", msgBuf);
                    return; 
                }
            }
            else if (answer == 'A' || answer == 'B' || answer == 'C' || answer == 'D') {
                if (answer == gameDeck[i].correctAnswer) {
                    answered = 1; 
                } else {
                    allowedAttempts--; 
                    
                    if (allowedAttempts > 0) {
                        sprintf(msgBuf, "Double Dip active! You have 1 attempt remaining, %s. Try again!", p.name);
                        gui_msgbox("WRONG ANSWER!", msgBuf);
                        PlaySound(TEXT("wrong.wav"), NULL, SND_FILENAME | SND_ASYNC); 
                    } else {
                        sprintf(msgBuf, "Oh no, %s! The correct answer was %c.\n>>> You take home Rs. %d. Better luck next time! <<<", p.name, gameDeck[i].correctAnswer, safePrize);
                        gui_msgbox("WRONG ANSWER!", msgBuf);
                        PlaySound(TEXT("wrong.wav"), NULL, SND_FILENAME | SND_SYNC); 
                        return; 
                    }
                }
            }
        } 

        PlaySound(TEXT("correct.wav"), NULL, SND_FILENAME | SND_ASYNC); 
        currentPrize = gameDeck[i].prizeMoney;
        sprintf(msgBuf, "*** CORRECT ANSWER! Congratulations %s! ***", p.name);
        gui_msgbox("Correct!", msgBuf);
        
        if (i == 4) { 
            safePrize = 10000;
            gui_msgbox("Level 1 Cleared!", "!!! WELL DONE! YOU CROSSED LEVEL 1. GUARANTEED RS. 10,000 !!!");
        } else if (i == 9) { 
            safePrize = 320000;
            gui_msgbox("Level 2 Cleared!", "!!! INCREDIBLE! YOU CROSSED LEVEL 2. GUARANTEED RS. 3,20,000 !!!");
        }
    }
    
    sprintf(msgBuf, "CONGRATULATIONS %s! YOU HAVE WON RS. 1 CRORE!\n\nGRAND TOTAL: Rs. %d", p.name, 10000000);
    gui_msgbox("VICTORY!", msgBuf);
}

int loadQuestions(const char* filename, struct Question questions[]) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) return 0;
    int count = 0;
    char buffer[256]; 
    while (count < MAX_Q && fgets(questions[count].text, sizeof(questions[count].text), file)) {
        if (questions[count].text[0] == '\n' || questions[count].text[0] == '\0') continue;
        questions[count].text[strcspn(questions[count].text, "\n")] = 0;
        for(int i = 0; i < 4; i++) {
            fgets(questions[count].options[i], sizeof(questions[count].options[i]), file);
            questions[count].options[i][strcspn(questions[count].options[i], "\n")] = 0; 
        }
        fscanf(file, " %c", &questions[count].correctAnswer);
        int dummyPrize;
        fscanf(file, "%d", &dummyPrize); 
        fgets(buffer, sizeof(buffer), file); 
        count++;
    }
    fclose(file);
    return count;
}

void shuffleQuestions(struct Question array[], int n) {
    if (n > 1) {
        for (int i = 0; i < n - 1; i++) {
            int j = i + rand() % (n - i);
            struct Question t = array[j];
            array[j] = array[i];
            array[i] = t;
        }
    }
}

void printRules() {
    gui_msgbox("RULES", "- 15 questions to win Rs. 1 Crore.\n- You have 45 SECONDS to answer each question.\n- LIFELINE 1: 50-50 (Removes 2 wrong answers)\n- LIFELINE 2: Flip (Replaces the question entirely)\n- LIFELINE 3: Double Dip (Get 2 attempts to guess the answer)\n- LEVEL 1 SAFE HAVEN: Rs. 10,000 (After Q5)\n- LEVEL 2 SAFE HAVEN: Rs. 3,20,000 (After Q10)\n- Type 'Q' at any time to walk away with your current prize.");
}
