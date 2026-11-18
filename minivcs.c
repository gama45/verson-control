/*
 * Mini Version Control System (Git-like) - beginner friendly version
 *
 * Features:
 *   1. Create repository      5. View history
 *   2. Add file to tracking   6. Compare two versions
 *   3. Remove file            7. Restore a version
 *   4. Save version           8. Search version by ID
 *
 * How data is stored (inside the folder "minivcs_data"):
 *   tracked.txt  -> names of files being tracked (one per line)
 *   history.txt  -> one line per saved version: id|date|fileCount|message
 *   v1.dat, v2.dat ... -> snapshot (copy of all tracked files) of each version
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DATA_DIR     "minivcs_data"
#define TRACK_FILE   DATA_DIR "/tracked.txt"
#define HISTORY_FILE DATA_DIR "/history.txt"

#define MAX_FILES    20     /* max tracked files               */
#define MAX_VERSIONS 100    /* max saved versions              */
#define MAX_NAME     100    /* max length of a file name       */
#define MAX_MSG      100    /* max length of a version message */
#define MAX_LINE     256    /* max length of a line when comparing */

/* One file stored inside a snapshot */
typedef struct {
    char  name[MAX_NAME];
    long  size;
    char *data;             /* file contents (allocated with malloc) */
} SnapFile;

/* A snapshot = all files of one version, loaded in memory */
typedef struct {
    int      count;
    SnapFile files[MAX_FILES];
} Snapshot;

/* One line of the history list */
typedef struct {
    int  id;
    char date[20];
    int  fileCount;
    char message[MAX_MSG];
} Version;

/* ------------------------------------------------------------------ */
/* Small helper functions                                              */
/* ------------------------------------------------------------------ */

/* Read one line from the keyboard safely. Returns 0 if input ended. */
int readLine(char *buf, int size)
{
    if (fgets(buf, size, stdin) == NULL) {
        buf[0] = '\0';
        return 0;
    }
    buf[strcspn(buf, "\r\n")] = '\0';   /* remove the newline */
    return 1;
}

/* Check whether the repository folder has been created */
int repoExists(void)
{
    FILE *fp = fopen(TRACK_FILE, "r");
    if (fp == NULL)
        return 0;
    fclose(fp);
    return 1;
}

/* Build the file name of a snapshot, e.g. id 3 -> "minivcs_data/v3.dat" */
void snapshotPath(int id, char *path)
{
    sprintf(path, "%s/v%d.dat", DATA_DIR, id);
}

/* Read a whole file into memory. Returns NULL if it cannot be opened. */
char *readWholeFile(const char *name, long *size)
{
    FILE *fp = fopen(name, "rb");
    char *buffer;

    if (fp == NULL)
        return NULL;

    fseek(fp, 0, SEEK_END);
    *size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    buffer = (char *)malloc(*size + 1);   /* +1 so size 0 still works */
    if (buffer != NULL)
        fread(buffer, 1, *size, fp);
    fclose(fp);
    return buffer;
}

/* ------------------------------------------------------------------ */
/* Tracked file list (tracked.txt)                                     */
/* ------------------------------------------------------------------ */

/* Load tracked file names into the array. Returns how many were read. */
int loadTracked(char list[][MAX_NAME])
{
    FILE *fp = fopen(TRACK_FILE, "r");
    int n = 0;

    if (fp == NULL)
        return 0;
    while (n < MAX_FILES && fgets(list[n], MAX_NAME, fp) != NULL) {
        list[n][strcspn(list[n], "\r\n")] = '\0';
        if (list[n][0] != '\0')
            n++;
    }
    fclose(fp);
    return n;
}

/* Write the array of tracked names back to tracked.txt */
void saveTracked(char list[][MAX_NAME], int n)
{
    FILE *fp = fopen(TRACK_FILE, "w");
    int i;

    if (fp == NULL)
        return;
    for (i = 0; i < n; i++)
        fprintf(fp, "%s\n", list[i]);
    fclose(fp);
}

/* Return the position of a name in the list, or -1 if not found */
int findName(char list[][MAX_NAME], int n, const char *name)
{
    int i;
    for (i = 0; i < n; i++)
        if (strcmp(list[i], name) == 0)
            return i;
    return -1;
}

/* ------------------------------------------------------------------ */
/* History list (history.txt)                                          */
/* ------------------------------------------------------------------ */

/* Load all versions. Returns how many were read. */
int loadHistory(Version v[])
{
    FILE *fp = fopen(HISTORY_FILE, "r");
    char line[300];
    int n = 0;

    if (fp == NULL)
        return 0;
    while (n < MAX_VERSIONS && fgets(line, sizeof(line), fp) != NULL) {
        /* line format: id|date|fileCount|message */
        if (sscanf(line, "%d|%19[^|]|%d|%99[^\n]",
                   &v[n].id, v[n].date, &v[n].fileCount, v[n].message) == 4)
            n++;
    }
    fclose(fp);
    return n;
}

/* ------------------------------------------------------------------ */
/* Snapshot loading / freeing                                          */
/* ------------------------------------------------------------------ */

/* Load snapshot of version 'id' into memory. Returns 1 on success. */
int loadSnapshot(int id, Snapshot *snap)
{
    char path[100];
    FILE *fp;
    int i;

    snap->count = 0;
    snapshotPath(id, path);
    fp = fopen(path, "rb");
    if (fp == NULL)
        return 0;

    fread(&snap->count, sizeof(int), 1, fp);
    for (i = 0; i < snap->count; i++) {
        fread(snap->files[i].name, MAX_NAME, 1, fp);
        fread(&snap->files[i].size, sizeof(long), 1, fp);
        snap->files[i].data = (char *)malloc(snap->files[i].size + 1);
        fread(snap->files[i].data, 1, snap->files[i].size, fp);
    }
    fclose(fp);
    return 1;
}

/* Release the memory used by a snapshot */
void freeSnapshot(Snapshot *snap)
{
    int i;
    for (i = 0; i < snap->count; i++)
        free(snap->files[i].data);
    snap->count = 0;
}

/* Find a file inside a snapshot by name. Returns index or -1. */
int findInSnapshot(Snapshot *snap, const char *name)
{
    int i;
    for (i = 0; i < snap->count; i++)
        if (strcmp(snap->files[i].name, name) == 0)
            return i;
    return -1;
}

/* ------------------------------------------------------------------ */
/* 1. Create repository                                                */
/* ------------------------------------------------------------------ */
void createRepository(void)
{
    FILE *fp;

    if (repoExists()) {
        printf("Repository already exists.\n");
        return;
    }

    system("mkdir " DATA_DIR);          /* works on Windows and Linux */

    fp = fopen(TRACK_FILE, "w");        /* empty list of tracked files */
    if (fp == NULL) {
        printf("Error: could not create repository.\n");
        return;
    }
    fclose(fp);

    fp = fopen(HISTORY_FILE, "w");      /* empty history */
    if (fp != NULL)
        fclose(fp);

    printf("Repository created successfully.\n");
}

/* ------------------------------------------------------------------ */
/* 2. Add file                                                         */
/* ------------------------------------------------------------------ */
void addFile(void)
{
    char list[MAX_FILES][MAX_NAME];
    char name[MAX_NAME];
    int n;
    FILE *fp;

    n = loadTracked(list);
    printf("Enter file name to add: ");
    readLine(name, MAX_NAME);

    if (name[0] == '\0') {
        printf("No file name entered.\n");
        return;
    }
    fp = fopen(name, "rb");             /* does the file exist? */
    if (fp == NULL) {
        printf("Error: file '%s' does not exist.\n", name);
        return;
    }
    fclose(fp);

    if (findName(list, n, name) != -1) {
        printf("File is already tracked.\n");
        return;
    }
    if (n >= MAX_FILES) {
        printf("Error: cannot track more than %d files.\n", MAX_FILES);
        return;
    }

    strcpy(list[n], name);
    n++;
    saveTracked(list, n);
    printf("File '%s' added to tracking.\n", name);
}

/* ------------------------------------------------------------------ */
/* 3. Remove file                                                      */
/* ------------------------------------------------------------------ */
void removeFile(void)
{
    char list[MAX_FILES][MAX_NAME];
    char name[MAX_NAME];
    int n, pos, i;

    n = loadTracked(list);
    if (n == 0) {
        printf("No files are being tracked.\n");
        return;
    }

    printf("Tracked files:\n");
    for (i = 0; i < n; i++)
        printf("  %d. %s\n", i + 1, list[i]);

    printf("Enter file name to remove: ");
    readLine(name, MAX_NAME);

    pos = findName(list, n, name);
    if (pos == -1) {
        printf("File is not being tracked.\n");
        return;
    }

    /* shift the remaining names one place to the left */
    for (i = pos; i < n - 1; i++)
        strcpy(list[i], list[i + 1]);
    n--;
    saveTracked(list, n);
    printf("File '%s' removed from tracking (the file itself is not deleted).\n", name);
}

/* ------------------------------------------------------------------ */
/* 4. Save version                                                     */
/* ------------------------------------------------------------------ */
void saveVersion(void)
{
    char list[MAX_FILES][MAX_NAME];
    char message[MAX_MSG];
    char path[100], dateText[20];
    Version hist[MAX_VERSIONS];
    char name[MAX_NAME];
    int n, total, newId, saved = 0, i;
    FILE *fp;
    time_t now;

    n = loadTracked(list);
    if (n == 0) {
        printf("No files are being tracked. Add files first.\n");
        return;
    }
    total = loadHistory(hist);
    if (total >= MAX_VERSIONS) {
        printf("Error: version limit reached.\n");
        return;
    }

    /* new ID = last ID + 1 (first version is 1) */
    newId = (total == 0) ? 1 : hist[total - 1].id + 1;

    printf("Enter a short message for this version: ");
    readLine(message, MAX_MSG);
    if (message[0] == '\0')
        strcpy(message, "No message");
    message[strcspn(message, "|")] = '\0';   /* '|' is our separator */

    snapshotPath(newId, path);
    fp = fopen(path, "wb");
    if (fp == NULL) {
        printf("Error: could not create snapshot file.\n");
        return;
    }

    fwrite(&saved, sizeof(int), 1, fp);      /* placeholder for count */
    for (i = 0; i < n; i++) {
        long size;
        char *data = readWholeFile(list[i], &size);

        if (data == NULL) {
            printf("Warning: '%s' not found, skipped.\n", list[i]);
            continue;
        }
        memset(name, 0, MAX_NAME);
        strcpy(name, list[i]);
        fwrite(name, MAX_NAME, 1, fp);       /* file name   */
        fwrite(&size, sizeof(long), 1, fp);  /* file size   */
        fwrite(data, 1, size, fp);           /* file content */
        free(data);
        saved++;
    }
    fseek(fp, 0, SEEK_SET);
    fwrite(&saved, sizeof(int), 1, fp);      /* write the real count */
    fclose(fp);

    if (saved == 0) {
        remove(path);
        printf("No files could be saved. Version not created.\n");
        return;
    }

    /* add a line to history.txt */
    now = time(NULL);
    strftime(dateText, sizeof(dateText), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fp = fopen(HISTORY_FILE, "a");
    fprintf(fp, "%d|%s|%d|%s\n", newId, dateText, saved, message);
    fclose(fp);

    printf("Version %d saved (%d file(s)).\n", newId, saved);
}

/* ------------------------------------------------------------------ */
/* 5. View history                                                     */
/* ------------------------------------------------------------------ */
void viewHistory(void)
{
    Version hist[MAX_VERSIONS];
    int total, i;

    total = loadHistory(hist);
    if (total == 0) {
        printf("No versions saved yet.\n");
        return;
    }

    printf("\n%-5s %-20s %-6s %s\n", "ID", "Date", "Files", "Message");
    printf("------------------------------------------------------------\n");
    for (i = 0; i < total; i++)
        printf("%-5d %-20s %-6d %s\n",
               hist[i].id, hist[i].date, hist[i].fileCount, hist[i].message);
}

/* ------------------------------------------------------------------ */
/* 6. Compare versions                                                 */
/* ------------------------------------------------------------------ */

/*
 * Read the next line of a text buffer into 'line'.
 * 'pos' remembers where we are. Returns 0 when there are no more lines.
 */
int nextLine(const char *data, long size, long *pos, char *line)
{
    int i = 0;

    if (*pos >= size)
        return 0;
    while (*pos < size && data[*pos] != '\n') {
        if (i < MAX_LINE - 1 && data[*pos] != '\r')
            line[i++] = data[*pos];
        (*pos)++;
    }
    if (*pos < size)
        (*pos)++;                        /* skip the '\n' */
    line[i] = '\0';
    return 1;
}

/* Show the lines that differ between two files (same line numbers) */
void showLineDifferences(SnapFile *a, SnapFile *b)
{
    char lineA[MAX_LINE], lineB[MAX_LINE];
    long posA = 0, posB = 0;
    int lineNo = 0, hasA, hasB;

    while (1) {
        hasA = nextLine(a->data, a->size, &posA, lineA);
        hasB = nextLine(b->data, b->size, &posB, lineB);
        if (!hasA && !hasB)
            break;
        lineNo++;
        if (hasA && hasB && strcmp(lineA, lineB) == 0)
            continue;                    /* same line, nothing to show */

        printf("      Line %d:\n", lineNo);
        printf("        - %s\n", hasA ? lineA : "(no line)");
        printf("        + %s\n", hasB ? lineB : "(no line)");
    }
}

void compareVersions(void)
{
    Snapshot s1, s2;
    char input[20];
    int id1, id2, i, j, differences = 0;

    printf("Enter first version ID: ");
    readLine(input, sizeof(input));
    id1 = atoi(input);
    printf("Enter second version ID: ");
    readLine(input, sizeof(input));
    id2 = atoi(input);

    if (!loadSnapshot(id1, &s1)) {
        printf("Version %d not found.\n", id1);
        return;
    }
    if (!loadSnapshot(id2, &s2)) {
        printf("Version %d not found.\n", id2);
        freeSnapshot(&s1);
        return;
    }

    printf("\nComparing version %d -> version %d\n", id1, id2);

    /* files in version 1: removed or modified? */
    for (i = 0; i < s1.count; i++) {
        j = findInSnapshot(&s2, s1.files[i].name);
        if (j == -1) {
            printf("  [REMOVED]  %s\n", s1.files[i].name);
            differences++;
        } else if (s1.files[i].size != s2.files[j].size ||
                   memcmp(s1.files[i].data, s2.files[j].data, s1.files[i].size) != 0) {
            printf("  [MODIFIED] %s\n", s1.files[i].name);
            showLineDifferences(&s1.files[i], &s2.files[j]);
            differences++;
        }
    }
    /* files only in version 2: added */
    for (j = 0; j < s2.count; j++) {
        if (findInSnapshot(&s1, s2.files[j].name) == -1) {
            printf("  [ADDED]    %s\n", s2.files[j].name);
            differences++;
        }
    }

    if (differences == 0)
        printf("  No differences. Both versions are identical.\n");

    freeSnapshot(&s1);
    freeSnapshot(&s2);
}

/* ------------------------------------------------------------------ */
/* 7. Restore version                                                  */
/* ------------------------------------------------------------------ */
void restoreVersion(void)
{
    Snapshot snap;
    char input[20];
    int id, i;
    FILE *fp;

    printf("Enter version ID to restore: ");
    readLine(input, sizeof(input));
    id = atoi(input);

    if (!loadSnapshot(id, &snap)) {
        printf("Version %d not found.\n", id);
        return;
    }

    printf("This will overwrite your current files. Continue? (y/n): ");
    readLine(input, sizeof(input));
    if (input[0] != 'y' && input[0] != 'Y') {
        printf("Restore cancelled.\n");
        freeSnapshot(&snap);
        return;
    }

    for (i = 0; i < snap.count; i++) {
        fp = fopen(snap.files[i].name, "wb");
        if (fp == NULL) {
            printf("  Could not write '%s'.\n", snap.files[i].name);
            continue;
        }
        fwrite(snap.files[i].data, 1, snap.files[i].size, fp);
        fclose(fp);
        printf("  Restored: %s\n", snap.files[i].name);
    }
    printf("Version %d restored.\n", id);
    freeSnapshot(&snap);
}

/* ------------------------------------------------------------------ */
/* 8. Search history by ID                                             */
/* ------------------------------------------------------------------ */
void searchVersion(void)
{
    Version hist[MAX_VERSIONS];
    Snapshot snap;
    char input[20];
    int total, id, i;

    total = loadHistory(hist);
    printf("Enter version ID to search: ");
    readLine(input, sizeof(input));
    id = atoi(input);

    /* simple linear search through the history array */
    for (i = 0; i < total; i++) {
        if (hist[i].id == id) {
            printf("\nVersion found!\n");
            printf("  ID      : %d\n", hist[i].id);
            printf("  Date    : %s\n", hist[i].date);
            printf("  Message : %s\n", hist[i].message);
            printf("  Files   : %d\n", hist[i].fileCount);
            if (loadSnapshot(id, &snap)) {
                int k;
                for (k = 0; k < snap.count; k++)
                    printf("    - %s (%ld bytes)\n",
                           snap.files[k].name, snap.files[k].size);
                freeSnapshot(&snap);
            }
            return;
        }
    }
    printf("Version %d not found.\n", id);
}

/* ------------------------------------------------------------------ */
/* Menu                                                                */
/* ------------------------------------------------------------------ */
void showMenu(void)
{
    printf("\n===== MINI VERSION CONTROL SYSTEM =====\n");
    printf("1. Create repository\n");
    printf("2. Add file\n");
    printf("3. Remove file\n");
    printf("4. Save version\n");
    printf("5. View history\n");
    printf("6. Compare versions\n");
    printf("7. Restore version\n");
    printf("8. Search version by ID\n");
    printf("0. Exit\n");
    printf("Enter your choice: ");
}

int main(void)
{
    char input[20];
    int choice;

    while (1) {
        showMenu();
        if (!readLine(input, sizeof(input)))
            break;                       /* input ended */
        choice = atoi(input);

        if (choice == 0) {
            printf("Goodbye!\n");
            break;
        }
        if (choice == 1) {
            createRepository();
            continue;
        }
        if (choice < 1 || choice > 8) {
            printf("Invalid choice. Try again.\n");
            continue;
        }
        /* every other option needs an existing repository */
        if (!repoExists()) {
            printf("No repository found. Choose option 1 first.\n");
            continue;
        }

        switch (choice) {
            case 2: addFile();         break;
            case 3: removeFile();      break;
            case 4: saveVersion();     break;
            case 5: viewHistory();     break;
            case 6: compareVersions(); break;
            case 7: restoreVersion();  break;
            case 8: searchVersion();   break;
        }
    }
    return 0;
}
