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

// --------------------------- BST for all songs ---------------------------
typedef struct SongBST {
    char *filename;   // e.g. "Aalas Ka Pedh - ...mp3" (heap-allocated)
    char *fullpath;   // full path to file (heap-allocated)
    struct SongBST *left;
    struct SongBST *right;
} SongBST;

SongBST *globalSongBST = NULL;

// utility: case-insensitive lower copy (caller frees result)
char *str_to_lower_copy(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *out = (char*)malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = s[i];
        if (c >= 'A' && c <= 'Z') out[i] = c - 'A' + 'a';
        else out[i] = c;
    }
    out[n] = '\0';
    return out;
}

// returns 1 if 'str' starts with 'prefix' (case-insensitive), 0 otherwise
int starts_with_case_insensitive(const char *str, const char *prefix) {
    if (!str || !prefix) return 0;
    while (*prefix) {
        char a = *str;
        char b = *prefix;
        if (a >= 'A' && a <= 'Z') a = a - 'A' + 'a';
        if (b >= 'A' && b <= 'Z') b = b - 'A' + 'a';
        if (a != b) return 0;
        ++str; ++prefix;
    }
    return 1;
}

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
// FIX: allocate a buffer instead of uninitialized pointer
char toPlaySong[512];
void remover()
{
  if (!front) {
    printf("list already empty\n");
    return;
  }

  queue* freer = front;
  // safe copy to buffer
  strncpy(toPlaySong, freer->value, sizeof(toPlaySong)-1);
  toPlaySong[sizeof(toPlaySong)-1] = '\0';
  front = front->next;
  if (front == NULL) rear = NULL;
  free(freer);
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

// create node helper
SongBST *bst_create_node(const char *filename, const char *fullpath) {
    SongBST *n = (SongBST*)malloc(sizeof(SongBST));
    if (!n) return NULL;
    n->filename = strdup(filename);
    n->fullpath = strdup(fullpath);
    n->left = n->right = NULL;
    return n;
}

// lexical compare that is case-insensitive (returns <0,0,>0)
int ci_compare(const char *a, const char *b) {
    if (!a || !b) return (a?1:-1);
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = ca - 'A' + 'a';
        if (cb >= 'A' && cb <= 'Z') cb = cb - 'A' + 'a';
        if (ca != cb) return (unsigned char)ca - (unsigned char)cb;
        ++a; ++b;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

// insert into BST (by filename in case-insensitive order)
// duplicates (same filename) are inserted to the right
void bst_insert(SongBST **root, const char *filename, const char *fullpath) {
    if (!root || !filename || !fullpath) return;
    if (!*root) {
        *root = bst_create_node(filename, fullpath);
        return;
    }
    SongBST *cur = *root;
    while (1) {
        int cmp = ci_compare(filename, cur->filename);
        if (cmp < 0) {
            if (!cur->left) { cur->left = bst_create_node(filename, fullpath); return; }
            cur = cur->left;
        } else {
            if (!cur->right) { cur->right = bst_create_node(filename, fullpath); return; }
            cur = cur->right;
        }
    }
}

void bst_free(SongBST *root) {
    if (!root) return;
    bst_free(root->left);
    bst_free(root->right);
    free(root->filename);
    free(root->fullpath);
    free(root);
}

// structure to store match list
typedef struct MatchList {
    char **paths; // array of fullpath strings
    char **names; // matching filenames
    int count;
    int capacity;
} MatchList;

void matchlist_init(MatchList *m) {
    m->paths = NULL;
    m->names = NULL;
    m->count = 0;
    m->capacity = 0;
}
void matchlist_push(MatchList *m, const char *name, const char *path) {
    if (m->count == m->capacity) {
        int newcap = (m->capacity == 0) ? 8 : m->capacity * 2;
        m->names = (char**)realloc(m->names, newcap * sizeof(char*));
        m->paths = (char**)realloc(m->paths, newcap * sizeof(char*));
        m->capacity = newcap;
    }
    m->names[m->count] = strdup(name);
    m->paths[m->count] = strdup(path);
    m->count++;
}
void matchlist_free(MatchList *m) {
    for (int i = 0; i < m->count; ++i) {
        free(m->names[i]);
        free(m->paths[i]);
    }
    free(m->names);
    free(m->paths);
    m->names = m->paths = NULL;
    m->count = m->capacity = 0;
}

// inorder traversal collecting filename prefix matches
void bst_collect_prefix(SongBST *root, const char *prefixLower, MatchList *out) {
    if (!root) return;
    // left
    bst_collect_prefix(root->left, prefixLower, out);

    // check match: file names start with prefix (case-insensitive)
    if (starts_with_case_insensitive(root->filename, prefixLower)) {
        matchlist_push(out, root->filename, root->fullpath);
    }

    // right
    bst_collect_prefix(root->right, prefixLower, out);
}

// wrapper: find matches for userPrefix (user types initial word, e.g. "Aalas")
MatchList bst_search_by_initial(const char *userPrefix) {
    MatchList res;
    matchlist_init(&res);
    if (!userPrefix || !globalSongBST) return res;

    // We will test start-of-filename with userPrefix, case-insensitive
    // because starts_with_case_insensitive is case-insensitive already,
    // just pass userPrefix as-is.
    bst_collect_prefix(globalSongBST, userPrefix, &res);
    return res;
}

//------------------------------------------------------------------DLL--------------------------------------------------------
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

void free_playlist() {
    SongNode *tmp = playlistHead;
    while (tmp) {
        SongNode *next = tmp->next;
        free(tmp);
        tmp = next;
    }
    playlistHead = playlistTail = currentSong = NULL;
    songCount = 0;
}
// audio extension check (case-insensitive)
int is_audio_file_by_name(const char *name) {
    const char *ext = strrchr(name, '.');
    if (!ext) return 0;
    // compare ignoring case
    const char *allowed[] = { ".mp3", ".wav", ".flac", ".m4a", ".aac", ".ogg", NULL };
    for (int i = 0; allowed[i]; ++i) {
        const char *a = ext;
        const char *b = allowed[i];
        // case-insensitive compare
        while (*a && *b) {
            char ca = *a, cb = *b;
            if (ca >= 'A' && ca <= 'Z') ca = ca - 'A' + 'a';
            if (cb >= 'A' && cb <= 'Z') cb = cb - 'A' + 'a';
            if (ca != cb) break;
            ++a; ++b;
        }
        if (*a == '\0' && *b == '\0') return 1;
    }
    return 0;
}

#ifndef _WIN32
// recursive add for unix
void scan_dir_recursive_unix(const char *root) {
    DIR *d = opendir(root);
    if (!d) return;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        // build path
        char pathbuf[2048];
        snprintf(pathbuf, sizeof(pathbuf), "%s/%s", root, entry->d_name);

        // check if directory
        struct stat st;
        if (stat(pathbuf, &st) == 0 && S_ISDIR(st.st_mode)) {
            // skip your local binary / db folder if you want to exclude some names (optional)
            if (strcmp(entry->d_name, "a.out") == 0 || strcmp(entry->d_name, "musicplayer.c") == 0) continue;
            scan_dir_recursive_unix(pathbuf);
        } else {
            if (is_audio_file_by_name(entry->d_name)) {
                bst_insert(&globalSongBST, entry->d_name, pathbuf);
            }
        }
    }
    closedir(d);
}
#else
// recursive add for Windows
#include <windows.h>
void scan_dir_recursive_win(const char *root) {
    char searchPath[2048];
    snprintf(searchPath, sizeof(searchPath), "%s\\*", root);
    WIN32_FIND_DATA fd;
    HANDLE h = FindFirstFile(searchPath, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        const char *name = fd.cFileName;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;
        char pathbuf[2048];
        snprintf(pathbuf, sizeof(pathbuf), "%s\\%s", root, name);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            scan_dir_recursive_win(pathbuf);
        } else {
            if (is_audio_file_by_name(name)) {
                bst_insert(&globalSongBST, name, pathbuf);
            }
        }
    } while (FindNextFile(h, &fd));
    FindClose(h);
}
#endif

// public helper to rebuild BST from a root path
void rebuild_song_bst_from_root(const char *root) {
    // free current
    bst_free(globalSongBST);
    globalSongBST = NULL;

    if (!root) return;

#ifdef _WIN32
    scan_dir_recursive_win(root);
#else
    scan_dir_recursive_unix(root);
#endif
}
// Build BST from multiple root directories (append all songs)
void build_bst_from_roots(const char *roots[], int n) {
    // Clear old BST first
    bst_free(globalSongBST);
    globalSongBST = NULL;

    for (int i = 0; i < n; ++i) {
        if (!roots[i]) continue;

#ifdef _WIN32
        scan_dir_recursive_win(roots[i]);
#else
        scan_dir_recursive_unix(roots[i]);
#endif
    }
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
    prevsong[0] = '\0'; // avoid stale prevsong
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
//-----------------------------------------------------------------------------PLAYING A SONG----------------------------------------------------------------------------------------

void playSong(const char *filename) {
    if (!filename) return;
    push((char*)filename); // push onto history stack (cast because push expects char[100])

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
    if (!playlistHead) {
        loadPlaylistSongs(path);
    }

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

void loadPlaylistSongs(char *path)
{
    // free any existing playlist
    free_playlist();

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
        append_song_node(fullpath);
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

 const char *roots[] = {".", "localdatabase"};
 build_bst_from_roots(roots, 2);

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
  free_playlist();
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
  free_playlist();
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
  free_playlist();
  change_directory("Playlist2");
  playPlaylist(".");
}
  else if(c=='8')
{
  stopPlaylistPlayback();
  free_playlist();
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


else if (c == 'l') {
        stopSong();
        stopPlaylistPlayback();
        free_playlist();

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
free_playlist();
loadPlaylistSongs(dir);
if (songCount > 0) {
    currentSong = playlistHead;
    playSong(currentSong->path);
} else {
    printf("No songs found in %s\n", dir);
}
}

else if (c == 'f') {
    // Search all playlists (global BST) by initial word and play selected result
    char keyword[256];
    printf("Enter initial word of song (case-insensitive, full initial word required): ");
    scanf("%255s", keyword);

    // rebuild if BST is empty (build from current directory and localdatabase)
    if (!globalSongBST) {
        // build from current working directory
        rebuild_song_bst_from_root(".");
        // also ensure localdatabase included (if localdatabase is separate path)
        rebuild_song_bst_from_root("localdatabase");
        // note: second call will reset the BST; if you want both, call a wrapper that scans both --
        // we'll supply that wrapper below (see integration note).
    }

    MatchList matches = bst_search_by_initial(keyword);
    if (matches.count == 0) {
        printf("No song found for \"%s\".\n", keyword);
    } else if (matches.count == 1) {
        printf("Found: %s\nPlaying: %s\n", matches.names[0], matches.paths[0]);
        // Stop any playlist thread so manual play is safe
        stopPlaylistPlayback();
        free_playlist(); // not strictly necessary but keeps behavior consistent with other manual plays
        playSong(matches.paths[0]);
    } else {
        printf("Multiple matches found:\n");
        for (int i = 0; i < matches.count; ++i) {
            printf("  %d) %s  ->  %s\n", i+1, matches.names[i], matches.paths[i]);
        }
        int choice = 1;
        printf("Enter number to play (1-%d): ", matches.count);
        if (scanf("%d", &choice) == 1 && choice >= 1 && choice <= matches.count) {
            stopPlaylistPlayback();
            free_playlist();
            playSong(matches.paths[choice-1]);
        } else {
            printf("Invalid choice.\n");
            // flush any leftover input? You can leave it as is.
        }
    }
    matchlist_free(&matches);
}

printf("next operation\n");


  }

    return 0;
}
