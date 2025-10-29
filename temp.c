//krishiv nairs project (threaded playlist version)
// Integrated threaded playlist playback (background) with ability to stop/skip

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <direct.h>
    #include <windows.h>
    #include <process.h>
    #include <mmsystem.h>
    #define MKDIR(dir) _mkdir(dir)
    #define RMDIR(dir) _rmdir(dir)
    #define CHDIR(dir) _chdir(dir)
    #define GETCWD(buf, size) _getcwd(buf, size)
    #pragma comment(lib, "winmm.lib")
    PROCESS_INFORMATION pi = {0}; // global process info for current playback
#else
    #include <pthread.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <unistd.h>
    #include <dirent.h>
    #include <signal.h>
    #include <sys/wait.h>
    #define MKDIR(dir) mkdir(dir, 0755)
    #define RMDIR(dir) rmdir(dir)
    #define CHDIR(dir) chdir(dir)
    #define GETCWD(buf, size) getcwd(buf, size)
    pid_t current_pid = 0; // global pid for current playback
#endif

// --------------------------- data structures -------------------------------
typedef struct stack{
    char operator[100];
    struct stack *next;
} stack;

stack* head=NULL;

typedef struct queue{
    char value[100];
    struct queue *next;
} queue;

queue* front=NULL;
queue* rear=NULL;

// --------------------------- queue helpers ---------------------------------
int lengthqueue() {
  queue* tmp=front;
  int i=0;
  while(tmp!=NULL) { tmp=tmp->next; ++i; }
  return i;
}

void adder(char val[100]) {
  queue* tmp=front;
  int length=lengthqueue();
  queue* p=(queue *)malloc(sizeof(queue));
  if(!p) return;
  if(length==0) {
    front=p; rear=p; strcpy(p->value,val); p->next=NULL;
  } else{
    while(tmp->next!=NULL) tmp=tmp->next;
    tmp->next=p; rear=p; p->next=NULL; strcpy(p->value,val);
  }
}

char toPlaySongBuf[512];
void remover() {
  queue* tmp=front;
  int length=lengthqueue();
  queue* freer;
  if(length!=0) {
    freer=tmp; tmp=tmp->next; strncpy(toPlaySongBuf,freer->value,sizeof(toPlaySongBuf)-1); toPlaySongBuf[sizeof(toPlaySongBuf)-1]='\0'; free(freer); front=tmp;
  } else {
    printf("list already empty\n"); 
  }
}

void traverseQueue() {
    queue *tmp=front;
    printf("queue contents - [");
    while(tmp!=NULL) { printf("%s, ",tmp->value); tmp=tmp->next; }
    printf("]\n");
}

// --------------------------- stack helpers --------------------------------
int lengthstack() { stack* tmp=head; int i=0; while(tmp!=NULL) { tmp=tmp->next; ++i; } return i; }

void push(char val[100]) {
  stack* p=(stack *)malloc(sizeof(stack)); if(!p) return;
  if(lengthstack()==0) { head=p; strcpy(p->operator,val); p->next=NULL; }
  else { p->next=head; head=p; strcpy(p->operator,val); }
}

char prevsong[100];
void pop() {
  stack* tmp=head; int length=lengthstack(); stack* freer;
  if(length==1) { freer=tmp; strcpy(prevsong,freer->operator); free(freer); head=NULL; }
  else if(length==0) { printf("list already empty\n"); }
  else { freer=head; head=freer->next; strcpy(prevsong,freer->operator); free(freer); }
}

void traverseStack() { stack *tmp=head; printf("stack contents - ["); while(tmp!=NULL) { printf("%s, ",tmp->operator); tmp=tmp->next; } printf("]\n"); }

// --------------------------- playback helpers ------------------------------

// Blocking play that updates the global process/pid so stopSong can terminate it
void playSongBlocking(const char *fullpath) {
#ifdef _WIN32
    STARTUPINFO si; PROCESS_INFORMATION localPi; ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si); ZeroMemory(&localPi, sizeof(localPi));
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "ffplay -nodisp -autoexit -loglevel quiet \"%s\"", fullpath);

    // If a previous global pi exists, ensure it's closed
    if (pi.hProcess != NULL) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        pi.hProcess = NULL; pi.hThread = NULL;
    }

    if (!CreateProcess(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &localPi)) {
        fprintf(stderr, "Error starting ffplay for %s (err=%lu)\n", fullpath, GetLastError());
        return;
    }

    // copy localPi into global pi so stopSong can kill it
    pi = localPi;

    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    pi.hProcess = NULL; pi.hThread = NULL;
#else
    // kill any leftover (shouldn't be needed, but safe)
    if (current_pid > 0) {
        // ensure no stale pid
        current_pid = 0;
    }

    pid_t pid = fork();
    if (pid == 0) {
        execlp("ffplay", "ffplay", "-nodisp", "-autoexit", "-loglevel", "quiet", fullpath, (char *)NULL);
        _exit(1);
    } else if (pid > 0) {
        current_pid = pid;
        int status;
        waitpid(pid, &status, 0);
        current_pid = 0;
    } else {
        perror("fork failed");
    }
#endif
}

// Non-blocking playSong kept for compatibility (starts and returns immediately)
void playSong(char *filename) {
#ifdef _WIN32
    // Stop previous song if running
    if (pi.hProcess != NULL) {
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        pi.hProcess = NULL;
        pi.hThread = NULL;
    }

    STARTUPINFO si = { sizeof(si) };
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "ffplay -nodisp -autoexit -loglevel quiet \"%s\"", filename);

    if (!CreateProcess(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        printf("Error starting ffplay\n");
    } else {
        printf("Now playing: %s\n", filename);
    }
#else
    if (current_pid > 0) {
        kill(current_pid, SIGKILL);
        current_pid = 0;
    }

    pid_t pid = fork();
    if (pid == 0) {
        execlp("ffplay", "ffplay", "-nodisp", "-autoexit", "-loglevel", "quiet", filename, NULL);
        _exit(1);
    } else if (pid > 0) {
        current_pid = pid;
        printf("Now playing: %s\n", filename);
    } else {
        perror("fork failed");
    }
#endif
    push(filename);
}

void stopSong() {
#ifdef _WIN32
    if (pi.hProcess != NULL) {
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        pi.hProcess = NULL;
        pi.hThread = NULL;
    }
#else
    if (current_pid > 0) {
        kill(current_pid, SIGKILL);
        current_pid = 0;
    }
#endif
}

// --------------------------- playlist (threaded) ---------------------------
static volatile int stopPlaylist = 0;
static char playlistPath[1024];

#ifdef _WIN32
unsigned __stdcall playlistThread(void *arg)
#else
void *playlistThread(void *arg)
#endif
{
    char *path = (char *)arg;
    stopPlaylist = 0;

#ifdef _WIN32
    WIN32_FIND_DATA findFileData;
    char searchPath[MAX_PATH];
    snprintf(searchPath, sizeof(searchPath), "%s\\*", path);

    HANDLE hFind = FindFirstFile(searchPath, &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Error: directory not found\n");
#ifdef _WIN32
        _endthreadex(0);
#else
        return NULL;
#endif
    }

    do {
        if (stopPlaylist) break;
        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

        // optionally filter audio extensions here (e.g., .mp3 .wav .flac)
        char fullPath[1024];
        snprintf(fullPath, sizeof(fullPath), "%s\\%s", path, findFileData.cFileName);
        printf("Now playing: %s\n", fullPath);
        playSongBlocking(fullPath);

    } while (FindNextFile(hFind, &findFileData));

    FindClose(hFind);
#else
    DIR *d = opendir(path);
    if (!d) { perror("Error opening directory"); return NULL; }
    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        if (stopPlaylist) break;
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;

        // optionally skip non-audio files
        char fullPath[1024];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, dir->d_name);
        printf("Now playing: %s\n", fullPath);
        playSongBlocking(fullPath);
    }
    closedir(d);
#endif

    printf("Playlist finished or stopped.\n");
#ifdef _WIN32
    _endthreadex(0);
#else
    return NULL;
#endif
}

void startPlaylistThread(const char *path) {
    strncpy(playlistPath, path, sizeof(playlistPath)-1);
    playlistPath[sizeof(playlistPath)-1] = '\0';
    stopPlaylist = 0;
#ifdef _WIN32
    uintptr_t thread = _beginthreadex(NULL, 0, playlistThread, (void *)playlistPath, 0, NULL);
    if (!thread) printf("Failed to create playlist thread\n"); else CloseHandle((HANDLE)thread);
#else
    pthread_t tid;
    if (pthread_create(&tid, NULL, playlistThread, (void *)playlistPath) != 0) perror("pthread_create failed"); else pthread_detach(tid);
#endif
}

void stopPlaylistPlayback() {
    stopPlaylist = 1;
    // also stop the currently-playing song immediately
    stopSong();
    printf("Stopping playlist...\n");
}

// --------------------------- directory helpers -----------------------------
void create_directory(char *dirname) { if (MKDIR(dirname) == 0) printf("Directory '%s' created successfully.\n", dirname); else perror("Error creating directory"); }
void remove_directory(const char *dirname) { if (RMDIR(dirname) == 0) printf("Directory '%s' removed successfully.\n", dirname); else perror("Error removing directory"); }

void list_directory(char *path) {
#ifdef _WIN32
    WIN32_FIND_DATA findFileData; char searchPath[260]; snprintf(searchPath, sizeof(searchPath), "%s\\*", path);
    HANDLE hFind = FindFirstFile(searchPath, &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) { perror("Error opening directory"); return; }
    printf("songs in '%s':\n", path);
    do {
        if (strcmp(findFileData.cFileName, ".") == 0 || strcmp(findFileData.cFileName, "..") == 0 ) continue;
        printf("  %s\n", findFileData.cFileName);
    } while (FindNextFile(hFind, &findFileData));
    FindClose(hFind);
#else
    DIR *d; struct dirent *dir; d = opendir(path); if (!d) { perror("Error opening directory"); return; }
    printf("songs in '%s':\n", path);
    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;
        printf("  %s\n", dir->d_name);
    }
    closedir(d);
#endif
}

void change_directory(char *dirname) { if (CHDIR(dirname) == 0) printf("Changed to playlist '%s'\n", dirname); else perror("Error changing playlist"); }
char cwd[1024];
char* print_current_directory() { if (GETCWD(cwd, sizeof(cwd)) != NULL) { printf("Current playlist: %s\n", cwd); } else { perror("getcwd() error"); } return cwd; }

void copyFromLocalDatabase(char *fileName,char *destFolder) {
    char sourcePath[512]; char destinationPath[512];
    snprintf(sourcePath, sizeof(sourcePath), "LOCALIFY/localdatabase/%s", fileName);
#ifdef _WIN32
    snprintf(destinationPath, sizeof(destinationPath), "%s\\%s", destFolder, fileName);
#else
    snprintf(destinationPath, sizeof(destinationPath), "%s/%s", destFolder, fileName);
#endif
    FILE *src = fopen(sourcePath, "rb"); if (!src) { perror("no such song in database"); return; }
    FILE *dest = fopen(destinationPath, "wb"); if (!dest) { perror("wrong playlist"); fclose(src); return; }
    char buffer[4096]; size_t bytes; while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) fwrite(buffer, 1, bytes, dest);
    fclose(src); fclose(dest);
    printf("File added to playlist successfully:\n   %s → %s\n", fileName, destFolder);
}

void searchSong(char *keyword) {
    const char *path = "./localdatabase";
    printf("Searching for \"%s\" in %s...\n", keyword, path);
#ifdef _WIN32
    WIN32_FIND_DATA findFileData; char searchPath[260]; snprintf(searchPath, sizeof(searchPath), "%s\\*", path);
    HANDLE hFind = FindFirstFile(searchPath, &findFileData); if (hFind == INVALID_HANDLE_VALUE) { perror("Error opening directory"); return; }
    int found = 0; do { if (strcmp(findFileData.cFileName, ".") == 0 || strcmp(findFileData.cFileName, "..") == 0) continue; if (strstr(findFileData.cFileName, keyword)) { printf("%s\n", findFileData.cFileName); found = 1; } } while (FindNextFile(hFind, &findFileData));
    FindClose(hFind); if (!found) printf("No matching songs found.\n");
#else
    DIR *d = opendir(path); if (!d) { perror("Error opening directory"); return; }
    struct dirent *dir; int found = 0; while ((dir = readdir(d)) != NULL) { if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue; if (strcasestr(dir->d_name, keyword)) { printf("%s\n", dir->d_name); found = 1; } }
    closedir(d); if (!found) printf("No matching songs found.\n");
#endif
}

// --------------------------- main menu ------------------------------------
int main()
{    
 char c;
 char playlist[100];
    
    printf("ENTER 1 FOR VIEWING ALL PLAYLISTS \nENTER 2 FOR GOING TO A PLAYLIST \nENTER 3 FOR PLAYING A PLAYLIST \nENTER 4 FOR CREATING A PLAYLIST\nENTER 5 FOR DELETING A PLAYLIST\nENTER 6 FOR GOING BACK\nENTER 7 FOR NON-LEAF NODES\nENTER 8 FOR PLAYING AOGE TUM KABHI \nENTER 9 FOR EXIT\n");
  while(1)
  {
   
    scanf(" %c" ,&c);
    if(c=='1')
    {
      list_directory(".");
    }
    else if(c=='2')
    {
      printf("Enter name of playlist you want to view");
      scanf("%s",playlist);
      change_directory(playlist);
      list_directory(playlist);
    }
    else if(c=='3')
    {
       searchSong("aoge");
    }
    else if(c=='4')
    {
     printf("Enter name of playlist you want to create");
     scanf("%s",playlist);
     create_directory(playlist);
    }
     else if(c=='5')
    {
      change_directory("Playlist1");
      playPlaylist(".");
    }
     else if(c=='6')
    {
      change_directory("..");
    }
      else if(c=='7')
    {
      change_directory("Playlist2");
      playPlaylist(".");
    }
      else if(c=='8')
    {
      change_directory("localdatabase");
      playSong("The Local Train - Aalas Ka Pedh - Aaoge Tum Kabhi (Official Audio).mp3");
      
    }
    else if(c=='9')
    {
      pop();
      pop();
      playSong(prevsong);
    }
    else if(c=='0')
    {
      stopSong();
      break;
    }
    printf("next operation\n");
  }



    return 0;
}


    
