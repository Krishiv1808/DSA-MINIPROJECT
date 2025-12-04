// krishiv nairs project
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX 200
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
//------------------------------------------------------------------------graph like representation---------------------------------------------------

typedef struct PlaylistList{
  char songName[MAX];
  struct PlaylistList* next;
} PlaylistList;

typedef struct adjPlaylist{
  char playlistName[MAX];
  struct adjPlaylist* next;
  PlaylistList* songHead;
  
} adjPlaylist;
void freePlaylistSongs(PlaylistList **head);
PlaylistList* playlHead=NULL;
adjPlaylist* adjHead=NULL;

void freeAdjacencyList() {
    adjPlaylist *p = adjHead;
    while (p) {
        adjPlaylist *nextPlaylist = p->next;

        freePlaylistSongs(&p->songHead);

        free(p);

        p = nextPlaylist;
    }

    adjHead = NULL;  
}

void addFolderToList(const char *folderPath) {
    adjPlaylist *n = (adjPlaylist*)malloc(sizeof(adjPlaylist));
    if (!n) return;

    // store FULL PATH here
    strncpy(n->playlistName, folderPath, sizeof(n->playlistName) - 1);
    n->playlistName[sizeof(n->playlistName) - 1] = '\0';

    n->songHead = NULL;
    n->next = NULL;

    if (adjHead == NULL) {
        adjHead = n;
    } else {
        adjPlaylist *tmp = adjHead;
        while (tmp->next) tmp = tmp->next;
        tmp->next = n;
    }
}
adjPlaylist* findPlaylistByPath(const char *path) {
    adjPlaylist *tmp = adjHead;
    while (tmp) {
        if (strcmp(tmp->playlistName, path) == 0) {
            return tmp;
        }
        tmp = tmp->next;
    }
    return NULL;
}
PlaylistList* createPlaylistSongNode(const char *songName) {
    PlaylistList *n = (PlaylistList*)malloc(sizeof(PlaylistList));
    if (!n) return NULL;
    strncpy(n->songName, songName, MAX - 1);
    n->songName[MAX - 1] = '\0';
    n->next = NULL;
    return n;
}
void freePlaylistSongs(PlaylistList **head) {
    PlaylistList *tmp = *head;
    while (tmp) {
        PlaylistList *next = tmp->next;
        free(tmp);
        tmp = next;
    }
    *head = NULL;
}
void appendSongToAdjPlaylist(adjPlaylist *pl, const char *songName) {
    if (!pl) return;
    PlaylistList *n = createPlaylistSongNode(songName);
    if (!n) return;

    if (pl->songHead == NULL) {
        pl->songHead = n;
    } else {
        PlaylistList *tmp = pl->songHead;
        while (tmp->next) tmp = tmp->next;
        tmp->next = n;
    }
}
void loadFoldersIntoList(const char *path)
{
    freeAdjacencyList();
#ifdef _WIN32
    WIN32_FIND_DATA findFileData;
    char searchPath[260];

    snprintf(searchPath, sizeof(searchPath), "%s\\*", path);

    HANDLE hFind = FindFirstFile(searchPath, &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (strcmp(findFileData.cFileName, ".") == 0 ||
                strcmp(findFileData.cFileName, "..") == 0)
                continue;

            char fullpath[512];
            snprintf(fullpath, sizeof(fullpath), "%s\\%s", path, findFileData.cFileName);

            // 1) add playlist node (adjacency list)
            addFolderToList(fullpath);

            // 2) load its songs into DLL + into that adjPlaylist’s songHead
            loadPlaylistSongs(fullpath);  
        }
    } while (FindNextFile(hFind, &findFileData));

    FindClose(hFind);

#else
    DIR *d = opendir(path);
    if (!d) return;

    struct dirent *dir;
    struct stat st;
    char fullpath[512];

    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") == 0 ||
            strcmp(dir->d_name, "..") == 0)
            continue;

        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, dir->d_name);

        if (stat(fullpath, &st) == 0 && S_ISDIR(st.st_mode)) {
            // 1) add playlist node
            addFolderToList(fullpath);

            // 2) load its songs
            loadPlaylistSongs(fullpath);  
        }
    }

    closedir(d);
#endif
}
void traversePlaylistsDFS() {
    adjPlaylist *p = adjHead;

    printf("\n=== Traversal of All Playlists and Their Songs ===\n");

    while (p != NULL) {
        printf("\nPlaylist: %s\n", p->playlistName);

        PlaylistList *s = p->songHead;
        if (s == NULL) {
            printf("   (No songs found)\n");
        } else {
            while (s != NULL) {
                printf("   - %s\n", s->songName);
                s = s->next;
            }
        }

        p = p->next;   
    }

    printf("\n======================================================\n");
}



//------------------------------------------------------------------------------STACK---------------------------------------------------------------------------
typedef struct stack{
    char operator[100];
    struct stack *next;
} stack;

stack* head=NULL;
stack* head1=NULL;
int lengthstack(stack* headloc)
{
  stack* tmp=headloc;
  int i=0;
  while(tmp!=NULL)
  {
    tmp=tmp->next;
    ++i;
  }
  return i;
}
void push(char val[100],stack** headloc)
{
  stack* tmp=*headloc;
  int length=lengthstack(*headloc);
  stack* p=(stack *)malloc(sizeof(stack));
  if(length==0)
  {
    *headloc=p;
    strcpy(p->operator,val);
    p->next=NULL;
  }
  else{
    p->next=*headloc;
    *headloc=p;
    strcpy(p->operator,val);
  }
}
char prevsong[100];
void pop(stack **headloc)
{
  stack* tmp=*headloc;

  int length=lengthstack(*headloc);
  stack* freer;
  if(length==1)
  {
    freer=tmp;
    strcpy(prevsong,freer->operator);
    free(freer);
    *headloc=NULL;
  }
  else if(length==0)
  {
    printf("list already empty\n");
    prevsong[0] = '\0'; // avoid stale prevsong
  }
  else
  {

    freer=*headloc;
    *headloc=freer->next;
    strcpy(prevsong,freer->operator);
    free(freer);
  }
}
void traverseStack(stack* headloc)
{
    stack *tmp=headloc;
    printf("stack contents - [");
    while(tmp!=NULL)
    {
        printf("%s, ",tmp->operator);
        tmp=tmp->next;
    }
    printf("]\n");

}
//---------------------------------MP3-------------------


int is_mp3_file(const char *filename) {
    int len = strlen(filename);
    if (len < 4) return 0;   // too short to be ".mp3"

    // check last 4 characters (case-insensitive)
    char a = filename[len-4];
    char b = filename[len-3];
    char c = filename[len-2];
    char d = filename[len-1];

    return  
        (a == '.' ) &&
        (b == 'm' || b == 'M') &&
        (c == 'p' || c == 'P') &&
        (d == '3');
}

//--------------------------------------------BST--------------------------------------

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


//-----------------------------------------------------------------------------PLAYING A SONG----------------------------------------------------------------------------------------

void playSong(const char *filename) {
    if (!filename) return;
    push((char*)filename,&head); // push onto history stack (cast because push expects char[100])

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

//-----------------------------------------------------------Playlist functions----------------------------------------------
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
void loadPlaylistSongs(char *path)
{
    // free any existing DLL playlist
    free_playlist();

    // find the adjacency node for this playlist path (if any)
    adjPlaylist *plNode = findPlaylistByPath(path);
    if (plNode) {
        // clear its existing song list so we don't duplicate entries
        freePlaylistSongs(&plNode->songHead);
    }

#ifdef _WIN32
    WIN32_FIND_DATA findFileData;
    char searchPath[260];
    snprintf(searchPath, sizeof(searchPath), "%s\\*", path);
    HANDLE hFind = FindFirstFile(searchPath, &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (strcmp(findFileData.cFileName, ".") == 0 || strcmp(findFileData.cFileName, "..") == 0) continue;

        // Only mp3 files
        if (!is_mp3_file(findFileData.cFileName))
            continue;

        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s\\%s", path, findFileData.cFileName);

        // 1) Add to current DLL playlist (for next/prev)
        append_song_node(fullpath);

        // 2) Also add to adjacency playlist song list
        if (plNode) {
            appendSongToAdjPlaylist(plNode, findFileData.cFileName); // store just file name
        }

    } while (FindNextFile(hFind, &findFileData));
    FindClose(hFind);

#else
    DIR *d = opendir(path);
    struct dirent *dir;
    if (!d) return;

    while ((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;

        // Only mp3 files
        if (!is_mp3_file(dir->d_name))
            continue;

        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, dir->d_name);

        // 1) Add to current DLL playlist (for next/prev)
        append_song_node(fullpath);

        // 2) Also add to adjacency playlist song list
        if (plNode) {
            appendSongToAdjPlaylist(plNode, dir->d_name); // just filename
        }
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
//----------------------------------------------------------------------------SYSTEM FUNCTIONS---------------------------------------------------------------------------------------
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


void copyFromLocalDatabase() {
    char prefix[256];
    char destFolder[256];
    char dstPath[512];

    printf("Enter song name (prefix): ");
    scanf("%255s", prefix);

    // Use your existing BST prefix search (returns MatchList)
    MatchList matches = bst_search_by_initial(prefix);

    if (matches.count == 0) {
        printf("No song found for \"%s\".\n", prefix);
        return;
    }

    // Show matches
    if (matches.count == 1) {
        printf("Found: %s\n", matches.names[0]);
    } else {
        printf("Multiple matches found:\n");
        for (int i = 0; i < matches.count; ++i) {
            printf("  %d) %s  ->  %s\n", i + 1, matches.names[i], matches.paths[i]);
        }
    }

    int choice = 1;
    if (matches.count > 1) {
        printf("Choose song number (1-%d): ", matches.count);
        if (scanf("%d", &choice) != 1 || choice < 1 || choice > matches.count) {
            printf("Invalid choice.\n");
            matchlist_free(&matches);
            return;
        }
    }

    // Ask destination playlist folder (relative)
    printf("Enter playlist folder name: ");
    scanf("%255s", destFolder);

#if _WIN32
    snprintf(dstPath, sizeof(dstPath), ".\\%s\\%s", destFolder, matches.names[choice - 1]);
#else
    snprintf(dstPath, sizeof(dstPath), "./%s/%s", destFolder, matches.names[choice - 1]);
#endif

    // Copy file from BST path to destination
    FILE *src = fopen(matches.paths[choice - 1], "rb");
    if (!src) {
        printf("Error: Cannot open source file.\n");
        matchlist_free(&matches);
        return;
    }

    FILE *dst = fopen(dstPath, "wb");
    if (!dst) {
        fclose(src);
        printf("Error: Cannot open destination.\n");
        matchlist_free(&matches);
        return;
    }

    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dst);
    }

    fclose(src);
    fclose(dst);

    printf("Song copied successfully → %s\n", dstPath);

    matchlist_free(&matches);
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

    //loadFoldersIntoList(".");

    const char *roots[] = {"localdatabase"};
    build_bst_from_roots(roots, 1);

   printf("V - VIEW ALL PLAYLISTS AND THEIR SONGS\nC - CREATE A NEW PLAYLIST\nP - LOAD AND PLAY SONGS FROM USER-ENTERED DIRECTORY\nF - SEARCH SONG AND PLAY\nE - SEARCH SONG AND ADD TO SOME PLAYLIST\nL - PLAY NEXT SONG IN CURRENT PLAYLIST\nJ - PLAY PREVIOUS SONG IN CURRENT PLAYLIST\nK - STOP\nA - TRAVERSE THE CURRENT PLAYLIST\n1 - GO BACK (CD ..)\n2 - CHANGE DIRECTORY TO DSA-MINIPROJECT\n--------------------------------------\nENTER YOUR CHOICE:\n"
);


    while (1)
    {
        scanf(" %c", &c);

        if (c == 'V')
        {
            loadFoldersIntoList(".");
            traversePlaylistsDFS();
            
        }
        /*else if (c == '2')
        {
            printf("Enter name of playlist you want to view");
            scanf("%s", playlist);
            stopPlaylistPlayback();
            free_playlist();
            change_directory(playlist);
            list_directory(playlist);
        }
        else if (c == '3')
        {
            searchSong("aoge");
        }*/
        else if (c == 'C')
        {
            printf("Enter name of playlist you want to create\n");
            scanf("%s", playlist);
            create_directory(playlist);
        }
        else if (c == '1')
        {
            change_directory("..");
        }
        else if (c == '2')
        {
            change_directory("DSA-MINIPROJECT");
        }
       
        else if (c == 'B')
        {
            pop(&head);
            push(prevsong, &head1);
            pop(&head);
            push(prevsong, &head1);
            playSong(prevsong);
        }
        else if (c == 'N')
        {
            pop(&head1);
            pop(&head1);
            playSong(prevsong);
        }
        else if (c == '0')
        {
            stopSong();
            break;
        }
        /*else if (c == 'L')
        {
            stopSong();
            stopPlaylistPlayback();
            free_playlist();

            loadPlaylistSongs(".");
            if (songCount > 0)
            {
                currentSong = playlistHead;
                playSong(currentSong->path);
            }
            else
            {
                printf("No songs found in this playlist.\n");
            }
        }*/
        else if (c == 'L')
        {
            stopSong();
            playNextSong();
        }
        else if (c == 'J')
        {
            stopSong();
            playPreviousSong();
        }
        else if (c == 'K')
        {
            stopSong();
        }
        else if (c == 'P')
        {
            char dir[512];
            printf("Enter playlist directory: ");
            scanf("%s", dir);
            stopPlaylistPlayback();
            free_playlist();
            loadPlaylistSongs(dir);

            if (songCount > 0)
            {
                currentSong = playlistHead;
                playSong(currentSong->path);
            }
            else
            {
                printf("No songs found in %s\n", dir);
            }
        }
        else if (c == 'E')
        {
          copyFromLocalDatabase();
        }
        else if (c == 'F')
        {
            char keyword[256];
            printf("Enter initial word of song (case-insensitive, full initial word required): ");
            scanf("%255s", keyword);

            if (!globalSongBST)
            {
                rebuild_song_bst_from_root("localdatabase");
            }

            MatchList matches = bst_search_by_initial(keyword);

            if (matches.count == 0)
            {
                printf("No song found for \"%s\".\n", keyword);
            }
            else if (matches.count == 1)
            {
                printf("Found: %s\nPlaying: %s\n", matches.paths[0], matches.names[0]);
                stopPlaylistPlayback();
                playSong(matches.paths[0]);
            }
            else
            {
                printf("Multiple matches found:\n");
                for (int i = 0; i < matches.count; ++i)
                {
                    printf("  %d) %s  ->  %s\n", i + 1, matches.paths[i], matches.names[i]);
                }

                int choice = 1;
                printf("Enter number to play (1-%d): ", matches.count);

                if (scanf("%d", &choice) == 1 && choice >= 1 && choice <= matches.count)
                {
                    stopPlaylistPlayback();
                    playSong(matches.paths[choice - 1]);
                }
                else
                {
                    printf("Invalid choice.\n");
                }
            }

            matchlist_free(&matches);
        }
        else if (c == 'T')
        {
            traverse_list();
        }
        else if (c == 'A')
        {
            copyFromLocalDatabase();
        }
        

        printf("next operation\n");
    }

    return 0;
}

