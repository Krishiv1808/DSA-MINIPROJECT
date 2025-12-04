// krishiv nairs project
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <direct.h>
    #include <windows.h>
    #include <process.h> // for _beginthread
    #define MKDIR(dir) _mkdir(dir)
    #define RMDIR(dir) _rmdir(dir)
    #define CHDIR(dir) _chdir(dir)
    #define GETCWD(buf, size) _getcwd(buf, size)
    #include <mmsystem.h>
    #pragma comment(lib, "winmm.lib")
    PROCESS_INFORMATION pi = {0};
#else
    #include <sys/stat.h>
    #include <sys/wait.h>   // fixed include spacing
    #include <sys/types.h>
    #include <unistd.h>
    #include <dirent.h>
    #include <signal.h>
    #include <pthread.h>
    #define MKDIR(dir) mkdir(dir, 0755)
    #define RMDIR(dir) rmdir(dir)
    #define CHDIR(dir) chdir(dir)
    #define GETCWD(buf, size) getcwd(buf, size)

    pid_t current_pid = 0; // moved after includes
#endif

#ifdef _WIN32
HANDLE playlistThread = NULL;
#else
pthread_t playlistThread;
#endif

int stopPlaylist = 0; // 1 = stop playback
int playlistThreadRunning = 0;

// ---------- Playlist as Doubly Linked List ----------
typedef struct SongNode {
    char path[512];
    struct SongNode *prev;
    struct SongNode *next;
} SongNode;

SongNode *playlistHead = NULL;
SongNode *playlistTail = NULL;
SongNode *currentSong = NULL; // pointer to currently playing song node
int songCount = 0;

void loadPlaylistSongs(char *path);
 void stopPlaylistPlayback();
//-------------------------------------------DLL--------------------------------------------------------
// ---------- Doubly-linked list helpers ----------
SongNode* create_song_node(const char *fullpath) {
    SongNode *n = (SongNode*)malloc(sizeof(SongNode));
    if (!n) return NULL;
    strncpy(n->path, fullpath, sizeof(n->path)-1);
    n->path[sizeof(n->path)-1] = '\0';
    n->prev = n->next = NULL;
    return n;
}

void append_song_node(const char *fullpath) {
    SongNode *n = create_song_node(fullpath);
    if (!n) return;
    if (!playlistHead) {
        playlistHead = playlistTail = n;
    } else {
        playlistTail->next = n;
        n->prev = playlistTail;
        playlistTail = n;
    }
    songCount++;
}

/*void free_playlist() {
    SongNode *tmp = playlistHead;
    while (tmp) {
        SongNode *next = tmp->next;
        free(tmp);
        tmp = next;
    }
    playlistHead = playlistTail = currentSong = NULL;
    songCount = 0;
}*/
void traverse_list()
{
  SongNode *tmp = playlistHead;
  printf("Songs currently in list- \n");
  
    while (tmp) {
        SongNode *next = tmp->next;
        printf("%s\n",tmp->path);
        tmp = next;
    }
}
//------------------------------------------------------------------------graph like representation---------------------------------------------------

typedef struct PlaylistList{
  char songName;
  struct PlaylistList* next;
} PlaylistList;

typedef struct adjancencyPlaylist{
  char playlistName;
  struct adjancencyPlaylist* next;
  PlaylistList* songHead;
  
} adjacencyPlaylist;
adjacencyPlaylist* adjHead;


//-----------------------------------------------------------------------------PLAYING A SONG----------------------------------------------------------------------------------------

void playSong(const char *filename) {
    if (!filename) return;
    //push((char*)filename); // push onto history stack (cast because push expects char[100])

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
    snprintf(cmd, sizeof(cmd),
             "ffplay -nodisp -autoexit -loglevel quiet \"%s\"", filename);

    if (!CreateProcess(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        printf("Error starting ffplay\n");
    } else {
        printf("Now playing: %s\n", filename);
    }

#else
    // Stop previous song if running
    if (current_pid > 0) {
        kill(current_pid, SIGKILL);
        current_pid = 0;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Child process: play song
        execlp("ffplay", "ffplay", "-nodisp", "-autoexit", "-loglevel", "quiet", filename, NULL);
        _exit(1); // exec failed
    } else if (pid > 0) {
        current_pid = pid;
        printf("Now playing: %s\n", filename);
    } else {
        perror("fork failed");
    }
#endif

    // (DO NOT push again here)
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

// safe play next that handles background playlist thread and unloaded playlist
// Play next, but do NOT loop past the last song — print message instead.
void playNextSong()
{
    if (songCount == 0 || !currentSong) {
        printf("No songs loaded. Please load playlist first.\n");
        return;
    }

    // If a background playlist thread is running, stop it first so it doesn't race with manual control
    if (playlistThreadRunning) {
        printf("[INFO] Stopping background playlist before manually advancing.\n");
        stopPlaylistPlayback();
    }

    // stop the currently playing ffplay process
    stopSong();

    // If there is no next node, inform the user and do not change currentSong
    if (!currentSong->next) {
        printf("No more songs in this playlist.\n");
        return;
    }

    // advance pointer
    currentSong = currentSong->next;

    printf("[INFO] playNextSong -> %s\n", currentSong->path);
    playSong(currentSong->path);
}

// Play previous, but do NOT loop before the first song — print message instead.
void playPreviousSong()
{
    if (songCount == 0 || !currentSong) {
        printf("No songs loaded. Please load playlist first.\n");
        return;
    }

    // If a background playlist thread is running, stop it first
    if (playlistThreadRunning) {
        printf("[INFO] Stopping background playlist before manually going previous.\n");
        stopPlaylistPlayback();
    }

    // stop the currently playing ffplay process
    stopSong();

    // If there is no previous node, inform the user and do not change currentSong
    if (!currentSong->prev) {
        printf("Already at first song in this playlist.\n");
        return;
    }

    currentSong = currentSong->prev;

    printf("[INFO] playPreviousSong -> %s\n", currentSong->path);
    playSong(currentSong->path);
}


// Blocking play starts ffplay and waits until it exits.
// Uses global pi/current_pid so stopSong() can terminate it.
void playSongBlocking(const char *filename) {
#ifdef _WIN32
    STARTUPINFO si;
    PROCESS_INFORMATION localPi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&localPi, sizeof(localPi));

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "ffplay -nodisp -autoexit -loglevel quiet \"%s\"", filename);

    // Close any previous global handles safely (they should already be closed by stopSong normally)
    if (pi.hProcess != NULL) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        pi.hProcess = NULL;
        pi.hThread = NULL;
    }

    if (!CreateProcess(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &localPi)) {
        fprintf(stderr, "Error starting ffplay for %s (err=%lu)\n", filename, GetLastError());
        return;
    }

    // assign to global so stopSong() can stop it
    pi = localPi;

    // wait until ffplay exits or gets killed
    WaitForSingleObject(pi.hProcess, INFINITE);

    // cleanup
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    pi.hProcess = NULL;
    pi.hThread = NULL;
#else
    // fork and wait so this function blocks until song finishes
    pid_t pid = fork();
    if (pid == 0) {
        // child
        execlp("ffplay", "ffplay", "-nodisp", "-autoexit", "-loglevel", "quiet", filename, (char *)NULL);
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

//-----------------------------------------------------------Threaded Playlist function----------------------------------------------
#ifdef _WIN32
unsigned __stdcall playlistThreadFunc(void *arg)
#else
void *playlistThreadFunc(void *arg)
#endif
{
    char *path = (char *)arg;

    // Mark running
    playlistThreadRunning = 1;

    // If playlist not loaded yet, load it from the given path
    //if (!playlistHead) 
        loadPlaylistSongs(path);
  

    SongNode *node = playlistHead;
    while (node && !stopPlaylist) {
        // Blocking play so we wait until the track finishes
        playSongBlocking(node->path);

        if (stopPlaylist) break;

        node = node->next;
        if (!node) node = playlistHead; // loop the playlist indefinitely
    }

    // free the copied path we allocated in playPlaylist()
    free(path);

    // mark not running
    playlistThreadRunning = 0;

#ifdef _WIN32
    _endthreadex(0);
    return 0;
#else
    return NULL;
#endif
}
void loadPlaylistSongs(char *path)
{
    // free any existing playlist
    //free_playlist();

#ifdef _WIN32
    WIN32_FIND_DATA findFileData;
    char searchPath[260];
    snprintf(searchPath, sizeof(searchPath), "%s\\*", path);
    HANDLE hFind = FindFirstFile(searchPath, &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (strcmp(findFileData.cFileName, ".") == 0 || strcmp(findFileData.cFileName, "..") == 0) continue;

        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s\\%s", path, findFileData.cFileName);
        append_song_node(fullpath);//appending to list
    } while (FindNextFile(hFind, &findFileData));
    FindClose(hFind);

#else
    DIR *d = opendir(path);
    struct dirent *dir;
    if (!d) return;
    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;

        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, dir->d_name);
        append_song_node(fullpath);
    }
    closedir(d);
#endif

    // set currentSong to head if songs loaded
    if (playlistHead) currentSong = playlistHead;
}
void playPlaylist(char *path) {
    stopPlaylist = 0; // reset stop flag

    // copy path for the thread, because arg must persist
    char *pathCopy = strdup(path);

#ifdef _WIN32
    if (playlistThread != NULL) {
        TerminateThread(playlistThread, 0);
        CloseHandle(playlistThread);
        playlistThread = NULL;
    }
    playlistThread = (HANDLE)_beginthreadex(NULL, 0, playlistThreadFunc, pathCopy, 0, NULL);
#else
    pthread_create(&playlistThread, NULL, playlistThreadFunc, pathCopy);
#endif

    printf("Started playlist in background.\n");
}


void stopPlaylistPlayback() {
    // signal the playlist loop to stop and stop current song
    stopPlaylist = 1;
    stopSong(); // kill currently playing ffplay immediately

#ifdef _WIN32
    if (playlistThread != NULL) {
        WaitForSingleObject(playlistThread, INFINITE);
        CloseHandle(playlistThread);
        playlistThread = NULL;
    }
#else
    if (playlistThreadRunning) {
        pthread_join(playlistThread, NULL);
    }
#endif

    // clear flag
    playlistThreadRunning = 0;
}
//--------------------------------------------------SYSTEM COMMANDS LIKE  RM,LS,ETC-----------------------------------------------------------
void create_directory(char *dirname) {
    if (MKDIR(dirname) == 0)
        printf("Directory '%s' created successfully.\n", dirname);
    else
        perror("Error creating directory");
}

void remove_directory(const char *dirname) {
    if (RMDIR(dirname) == 0)
        printf("Directory '%s' removed successfully.\n", dirname);
    else
        perror("Error removing directory");
}

void list_directory(char *path) {
#ifdef _WIN32
    WIN32_FIND_DATA findFileData;
    char searchPath[260];
    snprintf(searchPath, sizeof(searchPath), "%s\\*", path);

    HANDLE hFind = FindFirstFile(searchPath, &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        perror("Error opening directory");
        return;
    }

    printf("songs in '%s':\n", path);
    do {
    if (strcmp(findFileData.cFileName, ".") == 0 || strcmp(findFileData.cFileName, "..") == 0  || strcmp(findFileData.cFileName, "a.out") == 0 || strcmp(findFileData.cFileName, "musicplayer.c") == 0 || strcmp(findFileData.cFileName, "localdatabase") == 0)
        continue;

    printf("  %s\n", findFileData.cFileName);
} while (FindNextFile(hFind, &findFileData));


    FindClose(hFind);
#else
    DIR *d;
    struct dirent *dir;
    d = opendir(path);
    if (!d) {
        perror("Error opening directory");
        return;
    }

    printf("songs in '%s':\n", path);
    while ((dir = readdir(d)) != NULL) {
    if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0 || strcmp(dir->d_name, "a.out") == 0 || strcmp(dir->d_name, "musicplayer.c") == 0 || strcmp(dir->d_name, "localdatabase") == 0)
        continue;

    printf("  %s\n", dir->d_name);
}
    closedir(d);
#endif
}
void change_directory(char *dirname) {
    if (CHDIR(dirname) == 0)
        printf("Changed to playlist'%s'\n", dirname);
    else
        perror("Error changing playlist");
}
char cwd[1024];
char* print_current_directory() {

    if (GETCWD(cwd, sizeof(cwd)) != NULL)
    {

      printf("Current playlist: %s\n", cwd);

    }
    else
    {
      perror("getcwd() error");
    }
    return cwd;

}
//-----------------------------------------------------------------------------------------------------
void copyFromLocalDatabase(char *fileName,char *destFolder)
{
    char sourcePath[512];
    char destinationPath[512];


    snprintf(sourcePath, sizeof(sourcePath), "LOCALIFY/localdatabase/%s", fileName);


#ifdef _WIN32
    snprintf(destinationPath, sizeof(destinationPath), "%s\\%s", destFolder, fileName);
#else
    snprintf(destinationPath, sizeof(destinationPath), "%s/%s", destFolder, fileName);
#endif

    FILE *src = fopen(sourcePath, "rb");
    if (!src) {
        perror("no such song in database");
        return;
    }

    FILE *dest = fopen(destinationPath, "wb");
    if (!dest) {
        perror("wrong playlist");
        fclose(src);
        return;
    }

    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0)
        fwrite(buffer, 1, bytes, dest);

    fclose(src);
    fclose(dest);

    printf("File added to playlist successfully:\n   %s → %s\n", fileName, destFolder);
}
void searchSong(char *keyword)
{
    const char *path = "./localdatabase";
    printf("Searching for \"%s\" in %s...\n", keyword, path);

#ifdef _WIN32
    WIN32_FIND_DATA findFileData;
    char searchPath[260];
    snprintf(searchPath, sizeof(searchPath), "%s\\*", path);

    HANDLE hFind = FindFirstFile(searchPath, &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        perror("Error opening directory");
        return;
    }


    int found = 0;
    do {
        if (strcmp(findFileData.cFileName, ".") == 0 ||
            strcmp(findFileData.cFileName, "..") == 0)
            continue;

        if (strstr(findFileData.cFileName, keyword)) {
            printf("%s\n", findFileData.cFileName);
            found = 1;
        }
    } while (FindNextFile(hFind, &findFileData));

    FindClose(hFind);
    if (!found) printf("No matching songs found.\n");

#else
    DIR *d = opendir(path);
    if (!d) {
        perror("Error opening directory");
        return;
    }

    struct dirent *dir;
    int found = 0;
    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
            continue;

        if (strcasestr(dir->d_name, keyword)) {  // case-insensitive match
            printf("%s\n", dir->d_name);
            found = 1;
        }
    }
    closedir(d);
    if (!found) printf("No matching songs found.\n");
#endif
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
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
  stopPlaylistPlayback();
  //free_playlist();
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
  stopPlaylistPlayback();
  //free_playlist();
  change_directory("Playlist1");
  playPlaylist(".");
}
 else if(c=='6')
{
  change_directory("..");
}
  else if(c=='7')
{
  stopPlaylistPlayback();
  //free_playlist();
  change_directory("Playlist2");
  playPlaylist(".");
}
  else if(c=='8')
{
  stopPlaylistPlayback();
 // free_playlist();
  change_directory("localdatabase");
  playSong("The Local Train - Aalas Ka Pedh - Aaoge Tum Kabhi (Official Audio).mp3");

}
else if(c=='9')
{
  //pop();
  //pop();
  //playSong(prevsong);
}
else if(c=='0')
{
  stopSong();
  break;
}


else if (c == 'l') {
        stopSong();
        stopPlaylistPlayback();
        //free_playlist();

    loadPlaylistSongs(".");
    if (songCount > 0) {
        // updated for DLL: play first node
        currentSong = playlistHead;
        playSong(currentSong->path);
    } else {
        printf("No songs found in this playlist.\n");
    }
}
else if (c == 'k') {
        stopSong();
        

    playNextSong();
}
else if (c == 'j') {
        stopSong();
        
        playPreviousSong();


}
else if (c == 'i') {
stopSong();

}

else if (c == 'o') {
    char dir[512];
    printf("Enter playlist directory: ");
    scanf("%s", dir);
    stopPlaylistPlayback(); // ensure background playlist isn't using the old list
//free_playlist();
    loadPlaylistSongs(dir);
    if (songCount > 0) {
    currentSong = playlistHead;
    playSong(currentSong->path);
    }  
    else {
    printf("No songs found in %s\n", dir);
    }
}
else if (c == 'a') {
traverse_list();

}
printf("next operation\n");

  }

    return 0;
}
