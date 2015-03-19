#include "disk_emu.h"
#include "disk_emu.c"
#include "sfs_api.h"
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>


#define DISKSIZE 1000
#define MAXFILES 100
#define EMPTY (-2)


void printfat(void);
void printfdt(void);
void printroot(void);

/* Data Structures */
typedef struct
{
    char filename[32];
    int fat_index;
    time_t created;
    int size;
    int empty;
} disk_file;

typedef struct
{
    disk_file dir_table[MAXFILES];
    int next_cursor;
} root_dir;

typedef struct
{
    int db_index;
    int next;
} fat_node;

typedef struct
{
    fat_node fat_nodes[DISKSIZE];
    int next_cursor;
} fat_table;

typedef struct
{
    char filename[32];
    //int fileId;   // The index in the array of the fdt acts as the fileId
    int root_index;
    //fat_node root_fat;
    int opened;
    int wr_ptr;
    int rd_ptr;
} file_descriptor_table_node;

typedef struct
{
    // 0 indicates empty 
    // 1 indicates used
    int freeblocks[DISKSIZE];
} freeblocklist;


/* Global Variables */
root_dir root;
fat_table FAT;
freeblocklist freebl;
file_descriptor_table_node fdt[MAXFILES];

static int opened_files = 0;

static int rdbsize;  //       = sizeof(root_dir);
static int fatsize;    //     = sizeof(fat_table);
static int BLOCKSIZE;    //    = ( rdbsize > fatsize ? rdbsize : fatsize );


int main(void)
{
    mksfs(1);
    sfs_ls();
    int hell = sfs_fopen("hello");
    sfs_fopen("world");
    printroot();
    printfat();
    printfdt();

    char * a_w_buffer = "This is sparta!";
    char a_buffer[BLOCKSIZE];
    char read_buffer[BLOCKSIZE];

    printf("Before writing %s \n", a_w_buffer);

    sfs_fwrite(hell, "Hello This is the answer", 24);
    sfs_fwrite(hell, "Baby, I love", 13);

    printf("After Writing\n");

    close_disk();

    mksfs(0);

    sfs_fread(hell, read_buffer, 37);

    printf("\nread : \n%s\n\n", read_buffer);

    sfs_remove("hello");
    sfs_remove("world");

    printroot();
    printfat();
    printfdt();

    //write_blocks(2, 1, (void *)a_w_buffer);
    //read_blocks(2, 1, a_buffer);
    //printf("%s\nFinished\n", a_buffer);

    printf("HELLO THIS IS STARFLEET COMMAND\n");

    close_disk();
    return 0;
}



/* Here Starts the functions */
int mksfs(int fresh)
{
    rdbsize     = sizeof(root_dir);
    fatsize     = sizeof(fat_table);
    BLOCKSIZE   = ( rdbsize > fatsize ? rdbsize : fatsize );

    if (fresh) {
        init_fresh_disk("root.sfs", BLOCKSIZE, DISKSIZE);

        // Initialize files to empty in root dir
        int i;
        for(i = 1; i < MAXFILES; i++) {
            root.dir_table[i].empty = 1;
        }
        root.next_cursor = 0;
        // Initialize all blocks to empty
        for(i = 0; i < DISKSIZE; i++) {
            freebl.freeblocks[i] = 0;
        }
        for(i = 0; i < DISKSIZE; i++) {
            FAT.fat_nodes[i].db_index = EMPTY;
            FAT.fat_nodes[i].next = EMPTY;
        }
        FAT.next_cursor = 0;

        write_blocks( 0, 1, (void *)&root );
        write_blocks( 1, 1, (void *)&FAT );
        write_blocks(DISKSIZE-1, 1, (void *)&freebl);
    } else {
        init_disk("root.sfs", BLOCKSIZE, DISKSIZE);
        read_blocks( 0, 1, (void *)&root );
        read_blocks( 1, 1, (void *)&FAT );
        read_blocks( DISKSIZE-1, 1, (void *)&freebl );
    }
}

void sfs_ls()
{
    //root_dir root, *root_ptr;
    //read_blocks(0, 1, (void *)root_ptr);
    
    int i;
    for (i = 0; root.dir_table[i].empty == 0; i++) {
        int filesize = root.dir_table[i].size / 1000;
        printf("%s  %dKB  %s", root.dir_table[i].filename, 
                filesize, ctime(&root.dir_table[i].created) );
    }
}

int sfs_fopen(char * name)
{
    int nfileID = isopened(name);
    //printf("nfileID:%d\n", nfileID);
    if ( nfileID != -1) {
        return nfileID;
    }
    
    int root_dir_index = searchfile(name);

    // Add file to file descriptor table (fdt)
    strcpy( fdt[opened_files].filename, name );
    fdt[opened_files].opened = 1;
    fdt[opened_files].rd_ptr = 0;
    nfileID = opened_files;
    ++opened_files;
        //printf("root cursor: %d", root.next_cursor);
        //printf("root_dir_index: %d", root_dir_index);

    if ( root_dir_index == -1 ) {
        // Set write pointer to zero since new file
        fdt[opened_files].wr_ptr = 0;
        //printf("FAT cursor: %d\n", FAT.next_cursor);

        // Add new file to root_dir and FAT
        FAT.fat_nodes[FAT.next_cursor].db_index = get_next_freeblock();
        FAT.fat_nodes[FAT.next_cursor].next = -1;
        root.dir_table[ root.next_cursor ].empty = 0;
        strcpy( root.dir_table[ root.next_cursor ].filename, name ); 
        root.dir_table[ root.next_cursor ].fat_index = FAT.next_cursor;
        root.dir_table[ root.next_cursor ].size = 0;
        root.dir_table[ root.next_cursor ].created = time(NULL);

        //printroot();
        //printfat();

        fdt[nfileID].root_index = root.next_cursor;

        if (get_next_fat_cursor() == -1) exit(1);
        if (get_next_root_cursor() == -1) exit(1);
        write_blocks( 0, 1, (void *)&root );
        write_blocks( 1, 1, (void *)&FAT );
        write_blocks(DISKSIZE-1, 1, (void *)&freebl);
    } else {
        // Set write pointer at end of file
        fdt[nfileID].wr_ptr = root.dir_table[ root_dir_index ].size;
    }
    return nfileID;

}

int sfs_fclose(int fileID)
{
    if (opened_files <= fileID) {
        fprintf(stderr, "No such file %d", fileID);
    } else {
        fdt[ fileID ].opened = 0;
    }
    return 1;
}

int sfs_fwrite(int fileID, const char *buf, int length)
{
    if (opened_files <= fileID) {
        fprintf(stderr, "No such file %d is opened\n", fileID);
        return 1;
    }

    int nlength = length;

    int root_index = fdt[fileID].root_index;
    int fat_index = root.dir_table[ root_index ].fat_index;
    int db_index = FAT.fat_nodes[ fat_index ].db_index;

    while( FAT.fat_nodes[ fat_index ].next != -1 ) {
        db_index = FAT.fat_nodes[ fat_index ].db_index;
        fat_index = FAT.fat_nodes[ fat_index ].next;
    }
    
    // fill the current block first
    char temp_buffer[BLOCKSIZE];
    /*DEBUG*/
    //printf("\ndb_index: %d\n", db_index);
    //printf("\nfat_index : %d\n", fat_index);

    read_blocks(db_index, 1, (void *)temp_buffer);

    
    int write_pointer = size_in_block( fdt[ fileID ].wr_ptr ); //root.dir_table[ root_index ].size );
    if ( write_pointer != -1 ) {
        memcpy( (temp_buffer + write_pointer), buf, (BLOCKSIZE - write_pointer) );
        write_blocks( db_index, 1, (void *)temp_buffer );
        length = length - (BLOCKSIZE - write_pointer);
        buf = buf + (BLOCKSIZE - write_pointer);
    }


    // write the rest of the blocks
    while (length > 0) {
        memcpy( temp_buffer, buf, BLOCKSIZE );

        db_index = get_next_freeblock();

        FAT.fat_nodes[ fat_index ].next = FAT.next_cursor;
        FAT.fat_nodes[ FAT.next_cursor ].db_index = db_index;
        fat_index = FAT.next_cursor;
        FAT.fat_nodes[ fat_index ].next = -1;
        get_next_fat_cursor();

        /*DEBUG*/
        //printf("\ndb_index: %d\n", db_index);
        //printf("\nfat_index : %d\n", fat_index);


        length = length - BLOCKSIZE;
        //printf("length : %d\n", length);
        buf = buf + BLOCKSIZE;

        write_blocks( db_index, 1, (void *)temp_buffer );
    }

    root.dir_table[ root_index ].size += nlength;
    fdt[ fileID ].wr_ptr = root.dir_table[ root_index ].size;

    write_blocks( 0, 1, (void *)&root );
    write_blocks( 1, 1, (void *)&FAT );
    write_blocks(DISKSIZE-1, 1, (void *)&freebl);

    //printf("returning from sfs_fwrite\n");
    return;
}

int sfs_fread(int fileID, char * buf, int length)
{
    if (opened_files <= fileID && fdt[ fileID ].opened == 0 ) {
        fprintf(stderr, "No such file %d is opened\n", fileID);
        return 1;
    }

    char * buf_ptr = buf;

    char temp_buffer[BLOCKSIZE];

    int root_index = fdt[fileID].root_index;
    int fat_index = root.dir_table[ root_index ].fat_index;
    int db_index;
    db_index = FAT.fat_nodes[ fat_index ].db_index;

    // Get to the block with the current rd_ptr
    int read_pointer = size_in_block( fdt[ fileID ].rd_ptr );
    int rd_block_pointer = get_read_block( fdt[ fileID ].rd_ptr );
    //printf("rd_block_pointer: %d\n", rd_block_pointer);

    while (rd_block_pointer > 0) {
        if ( FAT.fat_nodes[ fat_index ].next != -1 ) {
            fat_index = FAT.fat_nodes[ fat_index ].next;
            --rd_block_pointer;
        }
    }

    db_index = FAT.fat_nodes[ fat_index ].db_index;

    // Read the first block
    read_blocks( db_index, 1, temp_buffer );
    memcpy(buf_ptr, (temp_buffer + read_pointer), (BLOCKSIZE - read_pointer));
    length = length - (BLOCKSIZE - read_pointer);
    buf_ptr = buf_ptr + (BLOCKSIZE - read_pointer);
    fat_index = FAT.fat_nodes[ fat_index ].next;

    // Read other blocks
    while( FAT.fat_nodes[ fat_index ].next != -1 && length > 0 && length > BLOCKSIZE ) {
        db_index = FAT.fat_nodes[ fat_index ].db_index;

        read_blocks(db_index, 1, temp_buffer);

        memcpy(buf_ptr, temp_buffer, BLOCKSIZE);

        length -= BLOCKSIZE;

        buf_ptr = buf_ptr + BLOCKSIZE;

        fat_index = FAT.fat_nodes[ fat_index ].next;
    }

    // Read last block
    db_index = FAT.fat_nodes[ fat_index ].db_index;
    read_blocks(db_index, 1, temp_buffer);
    memcpy(buf_ptr, temp_buffer, length);


    fdt[ fileID ].rd_ptr += length;
    return 1;
}

int sfs_fseek(int fileID, int loc)
{
    if (opened_files <= fileID) {
        fprintf(stderr, "No such file %d is opened\n", fileID);
        return 1;
    }
    fdt[ fileID ].wr_ptr = loc;
    fdt[ fileID ].rd_ptr = loc;
    return 1;
}

int sfs_remove(char * file)
{
    int temp_fat_index;
    int root_index = searchfile(file);
    if ( root_index == -1 ) return -1;
    int fat_index;

    // Remove from root dir
    root.dir_table[ root_index ].empty = 1;
    fat_index = root.dir_table[ root_index ].fat_index;

    // Remove from the FAT
    while( FAT.fat_nodes[ fat_index ].next != EMPTY ) {
        FAT.fat_nodes[ fat_index ].db_index = EMPTY;
        temp_fat_index = FAT.fat_nodes[ fat_index ].next;
        FAT.fat_nodes[ fat_index ].next = EMPTY;
        fat_index = temp_fat_index;
    }

    // Remove from FDT
    int fdt_index = isopened(file);
    if ( fdt_index != -1 ) {
        fdt[ fdt_index ].opened = 0;
    }

    return 0;
}

/*
 * Returns -1 if not opened or
 * returns FileID if opened
 * */

int isopened(char * name)
{
    int i;
    int result;
    for(i = 0; i < MAXFILES; i++) {
        //printf("filename: \"%s\", \"%s\"\n", fdt[i].filename, name);
        result = strcmp( fdt[i].filename, name );
        //printf("result:%d\n\n", result);
        if ( result == 0 ) {
            return i;
        }
    }
    return -1;
}

/*
 * Returns the index in the root dir if the file exists
 * else 
 * Returns -1
 * */

int searchfile(char * name)
{
    int i;
    for(i = 0; i < MAXFILES; i++) {
        if ( strcmp(root.dir_table[i].filename, name) == 0 ) {
            return i;
        }
    }
    return -1;
}

int get_next_freeblock()
{
    int i;
    for(i = 2; i < DISKSIZE - 1; i++) {
        if (freebl.freeblocks[i] == 0) {
            freebl.freeblocks[i] = 1;
            return i;
            //printf("next free block: %d\n", i);
        }
    }
    printf("WARNING DISK IS FULL; OPERATION CANNOT COMPLETE\n");
    printf("next free block: %d\n", i);
    return -1;
}

int get_next_fat_cursor()
{
    FAT.next_cursor += 1;
    if (FAT.next_cursor > DISKSIZE - 1) {
        int z;
        for (z = 0; z < DISKSIZE; z++) {
            if (FAT.fat_nodes[z].db_index == EMPTY) {
                break;
            }
        }
        FAT.next_cursor = z;
        if ( z == DISKSIZE ) {
            printf("WARNING DISK IS FULL; OPERATION CANNOT COMPLETE\n");
            return -1;
        }
    }
    return 0;
}

int get_next_root_cursor()
{
    root.next_cursor += 1;
    if (root.next_cursor > MAXFILES - 1) {
        int z;
        for (z = 0; z < MAXFILES; z++) {
            if (root.dir_table[z].empty == 1) {
                break;
            }
        }
        root.next_cursor = z;
        if (z == MAXFILES) {
            printf("WARNING MAX FILES CAPACITY REACHED\n");
            return -1;
        }
    }
    return 0;
}

/*
 * Returns the pointer in the last block
 * if the pointer is on the last byte of the block,
 * -1 is returned
 * */

int size_in_block(size)
{
    if (size == 0)return 0;
    if (size % BLOCKSIZE == 0){
        return -1;
    }
    while(size > BLOCKSIZE) {
        size -= BLOCKSIZE;
    }
    return size;
}

int get_read_block(size)
{
    int block = 0;
    while (size > BLOCKSIZE) {
        size -= BLOCKSIZE;
        ++block;
    }
    return block;
}

void printroot(void)
{
    printf("\nRoot Directory\n");
    int i;
    printf("filename\tfat_index\tsize\tempty\n");
    for (i = 0; i < MAXFILES; i++) {
        if ( root.dir_table[i].empty == 0 ) {
            
        }
    }
}

void printfat(void)
{
    printf("\nFAT\n");
    int i;
    printf("db_index\tnext\n");
    for (i = 0; i < DISKSIZE; i++) {
        if ( FAT.fat_nodes[i].db_index != EMPTY ) {
            printf("%d\t\t%d\n", FAT.fat_nodes[i].db_index, 
                    FAT.fat_nodes[i].next);
        }
    }
}

void printfdt(void)
{
    printf("\nFile Descriptor Table\n");
    printf("filename\troot_index\topened\twr_ptr\trd_ptr\n");
    int i;
    for (i = 0; i < MAXFILES; i++) {
        if(fdt[i].opened == 1) {
            printf("%s\t\t%d\t\t%d\t%d\t%d\n", fdt[i].filename, fdt[i].root_index,
                    fdt[i].opened, fdt[i].wr_ptr, fdt[i].rd_ptr);
        }
    }
}
