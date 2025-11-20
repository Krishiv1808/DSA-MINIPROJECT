//krishiv nairs project
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
    pid_t current_pid = 0;
    #include <sys/stat.h>   
    #include <sys/types.h>
    #include <unistd.h>     
    #include <dirent.h> 
    #include <signal.h>
    #include <pthread.h>   // 
    #define MKDIR(dir) mkdir(dir, 0755)
    #define RMDIR(dir) rmdir(dir)
    #define CHDIR(dir) chdir(dir)
    #define GETCWD(buf, size) getcwd(buf, size)
#endif

#ifdef _WIN32
HANDLE playlistThread = NULL;
#else
pthread_t playlistThread;
#endif

int stopPlaylist = 0; // 1 = stop playback
int playlistThreadRunning = 0;

typedef struct stack{

char operator[100];
struct stack *next;

}stack;

stack* head=NULL;
typedef struct queue{

char value[100];
struct queue *next;

}queue;

queue* front=NULL;
queue* rear=NULL;
//-------------------------------------------------------------------------QUEUE---------------------------------------------------------
int lengthqueue()
{
  queue* tmp=front;
  int i=0;
  while(tmp!=NULL)
  {
    tmp=tmp->next;
    ++i;
  }
  return i;
}
void adder(char val[100])
{
  queue* tmp=front;
  int length=lengthqueue();
  queue* p=(queue *)malloc(sizeof(queue));
  if(length==0)
  {
    front=p;
    rear=p;
    strcpy(p->value,val);
    p->next=NULL;
    
  }
  else{
    while(tmp->next!=NULL)
    {
      tmp=tmp->next;
    }
    tmp->next=p;
    rear=p;
    p->next=NULL;
    strcpy(p->value,val);
  }
}
char* toPlaySong[1024];
void remover()
{
  queue* tmp=front;
  int length=lengthqueue();
  queue* freer;
  if(length!=0)
  {
    freer=tmp;
    tmp=tmp->next;
    strcpy(toPlaySong,freer->value);
    free(freer);
    front=tmp;
  }
  else if(length==0)
  {
    printf("list already empty\n"); 
  }
  
}
void traverseQueue()
{
	queue *tmp=front;
	printf("queue contents - [");
	while(tmp!=NULL)
	{
		printf("%s, ",tmp->value);
		tmp=tmp->next;	
	}
	printf("]\n");

}
//------------------------------------------------------------------------------STACK---------------------------------------------------------------------------
int lengthstack()
{
  stack* tmp=head;
  int i=0;
  while(tmp!=NULL)
  {
    tmp=tmp->next;
    ++i;
  }
  return i;
}
void push(char val[100])
{
  stack* tmp=head;
  int length=lengthstack();
  stack* p=(stack *)malloc(sizeof(stack));
  if(length==0)
  {
    head=p;
    strcpy(p->operator,val);
    p->next=NULL;
  }
  else{
    p->next=head;
    head=p;
    strcpy(p->operator,val);
  }
}
char prevsong[100];
void pop()
{
  stack* tmp=head;
 
  int length=lengthstack();
  stack* freer;
  if(length==1)
  {
    freer=tmp;
    strcpy(prevsong,freer->operator);
    free(freer);
    head=NULL;
  }
  else if(length==0)
  {
    printf("list already empty\n");
  
  }
  else
  {
    
    freer=head;
    head=freer->next;
    strcpy(prevsong,freer->operator);
    free(freer);
  }
}
void traverseStack()
{
	stack *tmp=head;
	printf("stack contents - [");
	while(tmp!=NULL)
	{
		printf("%s, ",tmp->operator);
		tmp=tmp->next;	
	}
	printf("]\n");

}
/****************** Doubly-linked playlist structures & API ******************/

typedef struct dll_node {
    char filename[1024];         // filename only or full path if you prefer
    struct dll_node *prev;
    struct dll_node *next;
} dll_node;

typedef struct playlist {
    char name[256];             // friendly playlist name or directory path
    char path[1024];            // directory path for files
    dll_node *head;
    dll_node *tail;
    dll_node *current;          // currently selected node (for prev/next)
    int length;
} playlist;

/* API */
playlist *build_playlist_from_dir(const char *dirpath, const char *plist_name);
void free_playlist(playlist *pl);
void traverse_playlist(playlist *pl);
void play_current(playlist *pl);   // play pl->current
void play_next(playlist *pl);      // move current->next and play
void play_prev(playlist *pl);      // move current->prev and play

/************************* Playlist API implementation *************************/

#include <limits.h> /* for PATH_MAX if you want; not required */

static dll_node *create_node(const char *filename) {
    dll_node *n = (dll_node *)malloc(sizeof(dll_node));
    if (!n) return NULL;
    strncpy(n->filename, filename, sizeof(n->filename)-1);
    n->filename[sizeof(n->filename)-1] = '\0';
    n->prev = n->next = NULL;
    return n;
}

static void append_node_to_playlist(playlist *pl, dll_node *node) {
    if (!pl->head) {
        pl->head = pl->tail = node;
    } else {
        node->prev = pl->tail;
        pl->tail->next = node;
        pl->tail = node;
    }
    pl->length++;
}

/* Build playlist from directory (non-recursive). Returns malloc'd playlist (free later). */
playlist *build_playlist_from_dir(const char *dirpath, const char *plist_name) {
    if (!dirpath) return NULL;

    playlist *pl = (playlist *)malloc(sizeof(playlist));
    if (!pl) return NULL;
    memset(pl, 0, sizeof(playlist));
    strncpy(pl->path, dirpath, sizeof(pl->path)-1);
    if (plist_name) strncpy(pl->name, plist_name, sizeof(pl->name)-1);

#ifdef _WIN32
    WIN32_FIND_DATA findFileData;
    char searchPath[2048];
    snprintf(searchPath, sizeof(searchPath), "%s\\*", dirpath);

    HANDLE hFind = FindFirstFile(searchPath, &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        free(pl);
        return NULL;
    }

    do {
        if (strcmp(findFileData.cFileName, ".") == 0 || strcmp(findFileData.cFileName, "..") == 0)
            continue;
        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue; /* skip subdirs */

        dll_node *n = create_node(findFileData.cFileName);
        if (n) append_node_to_playlist(pl, n);
    } while (FindNextFile(hFind, &findFileData));
    FindClose(hFind);
#else
    DIR *d = opendir(dirpath);
    if (!d) { free(pl); return NULL; }

    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;
        /* skip directories */
        #ifdef DT_DIR
        if (dir->d_type == DT_DIR) continue;
        #endif
        dll_node *n = create_node(dir->d_name);
        if (n) append_node_to_playlist(pl, n);
    }
    closedir(d);
#endif

    /* initialize current to head so play_current works immediately if not empty */
    pl->current = pl->head;
    return pl;
}

void free_playlist(playlist *pl) {
    if (!pl) return;
    dll_node *it = pl->head;
    while (it) {
        dll_node *next = it->next;
        free(it);
        it = next;
    }
    free(pl);
}

void traverse_playlist(playlist *pl) {
    if (!pl) {
        printf("playlist is NULL\n");
        return;
    }
    printf("Playlist '%s' (%d items) at path: %s\n[", pl->name[0] ? pl->name : pl->path, pl->length, pl->path);
    dll_node *it = pl->head;
    while (it) {
        printf(" %s", it->filename);
        if (it->next) printf(",");
        it = it->next;
    }
    printf(" ]\n");
}

/* helper to build a full path for a node */
static void build_fullpath(char *out, size_t outlen, const char *dirpath, const char *filename) {
#ifdef _WIN32
    snprintf(out, outlen, "%s\\%s", dirpath, filename);
#else
    snprintf(out, outlen, "%s/%s", dirpath, filename);
#endif
}

/* Play the current node using your existing playSong (non-blocking).
   It will call push(filename) as your existing playSong does. */
void play_current(playlist *pl) {
    if (!pl || !pl->current) {
        printf("No current song to play.\n");
        return;
    }
    char fullpath[2048];
    build_fullpath(fullpath, sizeof(fullpath), pl->path, pl->current->filename);

    /* stop any existing player and play new one */
    stopSong();
    playSong(fullpath);
    printf("Playing current: %s\n", pl->current->filename);
}

/* Move to next and play */
void play_next(playlist *pl) {
    if (!pl) return;
    if (pl->current && pl->current->next) {
        pl->current = pl->current->next;
        play_current(pl);
    } else {
        printf("Already at end of playlist.\n");
    }
}

/* Move to prev and play */
void play_prev(playlist *pl) {
    if (!pl) return;
    if (pl->current && pl->current->prev) {
        pl->current = pl->current->prev;
        play_current(pl);
    } else {
        printf("Already at start of playlist.\n");
    }
}

//-----------------------------------------------------------------------------PLAYING A SONG----------------------------------------------------------------------------------------

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
// Blocking play: starts ffplay and waits until it exits.
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

#ifdef _WIN32
    WIN32_FIND_DATA findFileData;
    char searchPath[260];
    snprintf(searchPath, sizeof(searchPath), "%s\\*", path);

    HANDLE hFind = FindFirstFile(searchPath, &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        free(path);
        playlistThreadRunning = 0;
        return 0;
    }

    do {
        if (stopPlaylist) break;
        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        char fullPath[1024];
        snprintf(fullPath, sizeof(fullPath), "%s\\%s", path, findFileData.cFileName);
        playSongBlocking(fullPath);
    } while (FindNextFile(hFind, &findFileData));
    FindClose(hFind);

#else
    DIR *d;
    struct dirent *dir;
    d = opendir(path);
    if (!d) {
        free(path);
        playlistThreadRunning = 0;
        return NULL;
    }

    while ((dir = readdir(d)) != NULL) {
        if (stopPlaylist) break;
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
            continue;

        char fullPath[1024];
        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, dir->d_name);
        playSongBlocking(fullPath);
    }
    closedir(d);
#endif

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



//-----------------------------------------------------------------------------PLAYLIST MANAGER---------------------------------------------------------------------------------------
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

    /* NEW: Active in-memory playlist (doubly-linked) */
    playlist *activePl = NULL;
    char pldirname[1024];

    printf(
        "======== MUSIC PLAYER MENU ========\n"
        "1 : VIEW ALL PLAYLISTS (list current dir)\n"
        "2 : GO TO A PLAYLIST (cd & list)\n"
        "3 : SEARCH 'aoge' in localdatabase (demo)\n"
        "4 : CREATE A PLAYLIST (mkdir)\n"
        "5 : PLAY Playlist1 (change dir then play)\n"
        "6 : GO BACK (cd ..)\n"
        "7 : PLAY Playlist2 (change dir then play)\n"
        "8 : PLAY specific song from localdatabase (hardcoded)\n"
        "9 : POP twice and play prevsong (demo)\n"
        "0 : STOP SONG & EXIT\n\n"
        "a : LOAD PLAYLIST FROM DIRECTORY (build doubly-linked list)\n"
        "n : NEXT SONG in loaded playlist\n"
        "p : PREVIOUS SONG in loaded playlist\n"
        "l : LIST/SHOW active loaded playlist\n"
        "===================================\n"
        "Enter option: "
    );

    while (1)
    {
        /* read one non-whitespace char option safely */
        if (scanf(" %c", &c) != 1) {
            clearerr(stdin);
            continue;
        }

        if (c == '1')
        {
            list_directory(".");
        }
        else if (c == '2')
        {
            printf("Enter name of playlist you want to view: ");
            scanf("%99s", playlist);
            change_directory(playlist);
            list_directory(playlist);
        }
        else if (c == '3')
        {
            searchSong("aoge");
        }
        else if (c == '4')
        {
            printf("Enter name of playlist you want to create: ");
            scanf("%99s", playlist);
            create_directory(playlist);
        }
        else if (c == '5')
        {
            /* Original behaviour (kept as-is): change to Playlist1 then background play */
            change_directory("Playlist1");
            playPlaylist(".");
        }
        else if (c == '6')
        {
            change_directory("..");
        }
        else if (c == '7')
        {
            change_directory("Playlist2");
            playPlaylist(".");
        }
        else if (c == '8')
        {
            change_directory("localdatabase");
            playSong("The Local Train - Aalas Ka Pedh - Aaoge Tum Kabhi (Official Audio).mp3");
        }
        else if (c == '9')
        {
            pop();
            pop();
            playSong(prevsong);
        }
        else if (c == '0')
        {
            stopSong();
            /* cleanup and exit */
            if (activePl) {
                free_playlist(activePl);
                activePl = NULL;
            }
            break;
        }

        /* ---------- NEW LETTER COMMANDS FOR DOUBLY-LINKED PLAYLIST ---------- */

        else if (c == 'a') /* load playlist from directory */
        {
            printf("Enter playlist/directory name to load: ");
            scanf("%1023s", pldirname);

            if (activePl) {
                free_playlist(activePl);
                activePl = NULL;
            }
            activePl = build_playlist_from_dir(pldirname, pldirname);
            if (!activePl) {
                printf("Failed to load playlist from: %s\n", pldirname);
            } else {
                printf("Loaded playlist '%s' with %d songs.\n",
                       activePl->name[0] ? activePl->name : activePl->path,
                       activePl->length);
            }
        }
        else if (c == 'n') /* next */
        {
            if (!activePl) {
                printf("No playlist loaded. Use 'a' to load one.\n");
            } else {
                play_next(activePl);
            }
        }
        else if (c == 'p') /* previous */
        {
            if (!activePl) {
                printf("No playlist loaded. Use 'a' to load one.\n");
            } else {
                play_prev(activePl);
            }
        }
        else if (c == 'l') /* list active playlist */
        {
            if (!activePl) {
                printf("No active playlist.\n");
            } else {
                traverse_playlist(activePl);
            }
        }

        /* unknown option fallback */
        else
        {
            printf("Unknown option '%c'. Try again.\n", c);
        }

        printf("next operation\n");
    } /* end while */

    return 0;
}

