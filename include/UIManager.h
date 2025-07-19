#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <M5Core2.h>

class UIManager {
private:
  enum ScreenState {
    SCREEN_IDLE,
    SCREEN_HEARING,
    SCREEN_THINKING, 
    SCREEN_SPEAKING
  };
  
  ScreenState currentScreen;
  
public:
  UIManager();
  
  bool init();
  void showIdleScreen();
  void showHearingScreen();
  void showThinkingScreen();
  void showSpeakingScreen();
  
private:
  void changeScreen(ScreenState newScreen, const char* imagePath);
  bool loadImageIfExists(const char* imagePath);
};

#endif
