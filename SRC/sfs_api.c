#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "disk_emu.c"

// need some structure to define blocks on disk 
typedef struct{
	char *name ;
	int i_index;
	int pointers[12];// it confuses me why this is nessessary
}file; 


typedef struct{
	char mode;
	int link_count;
	int uid;
	int gid;
	int size;
	int cur_block;
	int pointers[13];
}i_node;

typedef struct{
	int magic;
	int block;
	int file_system_size;
	int i_node_length;
	int root_directory;
}super_block;

typedef struct{
	int used;
	int useless[12];// i dont know why but i will get a segmentation fault without this everytime
}block_map;

typedef struct{
	char *name;
	int reader_pointer;
	int writer_pointer;
	int dir_index;
	int open;
}des_entry ;


/*global variables*/

des_entry empty_des_entry;
block_map empty_block_map;
i_node empty_i_node;
file empty_file;
super_block s_block; 
file directory[100];
i_node i_table[100];
block_map freeblocks[500];
des_entry des_table[100];



int dir_pointer;// pointer to directory
int block_size;
int a[5];
int w;
char * hello;
char * moto;

int max ( int a, int b ) { return a > b ? a : b; }

int mksfs(int fresh){
	
	if(fresh==1){
		empty_file.i_index = -1;
		empty_des_entry.dir_index = -1;
		//now I do not have to worry about the root directory being larger the 1 block, it is always equal to or less then this size

		block_size = max(sizeof(i_table), (max( sizeof(directory),sizeof(freeblocks))));
			
		
		
		s_block.block= block_size;
		s_block.magic = 2864381957;// this is what 0xAABB0005 is in decimal
		s_block.file_system_size = 500;
		s_block.i_node_length = 1; // Guaranteed
		s_block.root_directory = 2; // by definition, i-node number = block number
		
		

		// must clear out disk 
		init_fresh_disk("disk.txt", block_size, 500);
		
		//must empty out the tables and chairs lol
		int i = 0;
		dir_pointer = 0;
		for( i= 0; i<500; i++){
			freeblocks[i]= empty_block_map;
		}
		freeblocks[0].used = 1;
		freeblocks[1].used = 1;
		freeblocks[2].used = 1;
		freeblocks[499].used = 1;
		
		empty_i_node.size = 0;
		empty_i_node.pointers[0] = -1;
		empty_file.pointers[0]=-1;

		for( i= 0; i<100; i++){
		des_table[i] = empty_des_entry;
		directory[i] = empty_file;
		i_table[i] = empty_i_node;
		}
		//make inode for root directory 
		i_node root;
		root.mode= 'a';
		root.link_count=0;
		root.uid= 0;
		root.gid = 0;
		root.size = sizeof(directory);
		root.pointers[0] = 2;
		i_table[0] = root;
		//writing over the disk space 
		
		write_blocks(0,1,(void *)&s_block);
		write_blocks(1,1,(void *)&i_table);
		write_blocks(2,1,(void *)&directory);	
        	write_blocks(499,1,(void *)&freeblocks);
		
	}

	 else
	 {
		init_disk("disk.txt",s_block.block,500);
		read_blocks(0,1,(void *)&s_block);
		read_blocks(1,1,(void *)&i_table  );
		read_blocks(2,1,(void *)&directory);
		read_blocks(500-1,1,(void *)&freeblocks);
      }
return 0;
}

int sfs_get_next_filename(char *fname){
	while(1){
		if(dir_pointer == 99){ return 0;}
		else{ 
			dir_pointer++;

			file k = directory[dir_pointer];
			if (k.i_index == -1) {//uninitialized 				
			continue;			
			}
			else{
			fname = (char *) malloc( sizeof(k.name));
			strncpy(fname, k.name,sizeof(k.name) );
			return 1;	
			}	
		}
	}
return 2;
}

int sfs_GetFileSize(char const *path){
	char *pos = strrchr(path, '\\');
	pos++;
	int i =0;
	while(1){
		
		if(i == 99){ return 0;}
		if(directory[i].i_index== -1) {i++;continue;}
		else{	
			if (strcmp(pos,directory[i].name)==0) return 					i_table[directory[i].i_index].size; 	
			else i++; 
			}	
		}
	}

int sfs_fopen(char *name){ 
	
 	if(exists(name)<100) return exists(name); //already opened 
	if(root_exists(name)<100){// situation where file exists on root and i-node table but not on  descriptor table
		int open = 0;
		for( open = 0; open <100; open++){
			if( des_table[open].dir_index == -1) break;
		}
		if (open== 100) return -1; // max number of files
		des_entry new;
		int i_root = root_exists2(name);
		new.name= (char *) malloc( sizeof(directory[i_root].name)+1);
		
		strcpy(new.name, directory[i_root].name);
		new.reader_pointer = i_table[i_root].pointers[0]*s_block.block;
		new.writer_pointer = i_table[i_root].pointers[0]*s_block.block + i_table[i_root].size;
		new.dir_index = i_root;
		new.open = 1;
		des_table[open]= new;
		return open;
	}
	else{
	// this is an entirely new file have to add to file desriptor table,the i done table, and the root dirctory table
		int open = 0;
		for( open = 0; open <100; open++){
			if( des_table[open].dir_index == -1) break;
		}
		
		if (open== 100) return -1; // max number of files
		des_entry new;
		new.name= (char * )malloc(sizeof(name));
		strcpy(new.name, name);
		new.reader_pointer = 0;
		new.writer_pointer = 0;
		new.open = 1;
		des_table[open]= new;
		// fill in inode info 
		
		int k =0; 
		for( k = 0; k <100; k++){
			if(i_table[k].pointers[0]==-1) break;
		}
		if (open== 100) return -1;
		i_table[k].mode = 'a';
		i_table[k].link_count= 0;
		i_table[k].uid= 0;
		i_table[k].gid =0;
		i_table[k].size = 0;
		int t = new_block();
		
		
		i_table[k].pointers[0]=t;
		
		i_table[k].pointers[1]=-1;
		i_table[k].cur_block= t;
		
		
		
		//fill in directory info
		
		
		int l =0; 
		for( l = 0; l <100; l++){
			if( directory[l].pointers[0] == -1) break;
		}
		if (l== 100) return -1;
		directory[l].name = (char*) malloc(100);
		directory[l].pointers[0]=1;
		strcpy(directory[l].name, name);
		
		directory[l].i_index =k;
		

		des_table[open].dir_index = l;
		write_blocks(0,1,(void *)&s_block);
		write_blocks(1,1,(void *)&i_table);
		write_blocks(2,1,(void *)&directory);	
        	write_blocks(499,1,(void *)&freeblocks);
		return open;
	}
}  

int sfs_fclose( int fileID){
	if( fileID> 100){printf("out of bounds");return 1;}
	else{	
		if( des_table[fileID].dir_index ==-1){printf("this case"); return 1;}
		des_entry new;
		new.dir_index = -1;
		des_table[fileID] = new;
		return 0;
	}

}

int sfs_fwrite(int  fileID,  const char  * buf,  int  length){//  as implied in the assignmnet you can only write to the end of the file" So if you are writing to an existing file, it is important you readthe last block and set the write pointer to the end "
	
 	if(des_table[fileID].dir_index == -1||des_table[fileID].open==0){ 
		printf("invaid fileID or file not open"); 
		return 9;
	}
	int saved = length;
	int i_node_index = directory[des_table[fileID].dir_index].i_index;
	char filler[s_block.block];
	read_blocks(i_table[i_node_index].cur_block,1,(void *)filler);
	int l;	
	int w_ptr = i_table[i_node_index].size % s_block.block;
	if( length < s_block.block- w_ptr){
	memcpy( (filler + w_ptr), buf, length);
	}
	else{ memcpy((filler+ w_ptr), buf, s_block.block- w_ptr);
	}
	
	buf = buf + s_block.block- w_ptr ;	
	write_blocks( i_table[i_node_index].cur_block,1, (void *)&filler);
	length = length - s_block.block +w_ptr;
	
	while(length > 0){// need to fill some blocks
		memcpy(filler, buf, s_block.block);
		int n = new_block();
		i_table[i_node_index].cur_block = n;
		length = length - s_block.block;
		buf= buf + s_block.block;
		int i = 0;
		
		for( i = 0; i<12; i++){
		if(i_table[i_node_index].pointers[i] ==-1) 				break;		
		}
		if ( i ==12){// the case where we use the single direct pointer
			if(i_table[i_node_index].pointers[12] ==-1){//new to used
				int new = new_block();
				i_table[i_node_index].pointers[12]= new;
				int array[100]; 
				array[0] = n;
				array[1] = -1;
				write_blocks(new,1,(void *)&array);
			}
			else{// need to update the pointer
				int new = i_table[i_node_index].pointers[12];
				int array[100];
				read_blocks(new, 1, (void *)&array);
				int m = 0;
				for( m = 0; m<100 ; m++){
					if(array[m] == -1) break; 	
				}				
				 if ( m == 100){printf( "file too long"); 							return 0;} 
				array[m+1] = -1; 
				array[m] = n;
				write_blocks(new,1,(void *)&array);

			}
		}
		else{
			i_table[i_node_index].pointers[i] = n; 
			i_table[i_node_index].pointers[i+1]= -1;
		
		}
		write_blocks( n,1,(void *) filler);


	}
	i_table[i_node_index].size = i_table[i_node_index].size + saved;
	des_table[fileID].writer_pointer = des_table[fileID].writer_pointer +saved;
	
	write_blocks(0,1,(void *)&s_block);
	write_blocks(1,1,(void *)&i_table);
	write_blocks(2,1,(void *)&directory);	
        write_blocks(499,1,(void *)&freeblocks);
	return saved;

}
int  sfs_fread(int  fileID, char  *buf,  int  length){// you can only read from the beggining of the file, at no point in the instrauction do you indecate to actually use the reader or writer pointers, and it is strongly impled nt to use the writer pointers. please email me if there is a problem with this. I  thought hard about this and it seemed like that is what you wanted as we cannot know just from the fileID and the writer pointer where the next block for the file will be.
   	if(des_table[fileID].dir_index == -1||des_table[fileID].open==0){ 
		printf("invaid fileID or file not open "); 
		return -1;
	}

	char  * buffer = buf ;
	int saved = length;
	int i_node_index = directory[des_table[fileID].dir_index].i_index;
	char filler[s_block.block];
	int l=0 , n = 0; 
	
	
	while(length> s_block.block){
		if ( n<12){
		int point = i_table[i_node_index].pointers[n];
		if ( point == -1){printf("file is not that long"); break;}
		read_blocks(point,1,(void*)&filler);
		memcpy(buffer, filler, s_block.block);
		n++;
		}
		else{
			int point = i_table[i_node_index].pointers[12];
			int array[100]; 
			read_blocks(point,1, (void *)&array);
			if ( array[l]== -1){printf("file is not that 					long"); return 1;}
			read_blocks(array[l],1,(void*)&filler);
			memcpy(buffer, filler, s_block.block);
			l++;
		}
		
		buffer = buffer + s_block.block;
		length= length- s_block.block;
	}
	if (n==12){

		int point = i_table[i_node_index].pointers[12];
		int array[100]; 
		read_blocks(point,1, (void *)&array);
		if ( array[l]== -1){printf("file is not that 				long"); return 1;}
		read_blocks(array[l],1,(void*)&filler);
		memcpy(buffer, filler, length);
	}
	else{
	
		int point = i_table[i_node_index].pointers[n];
		if ( point == -1){printf("file is not that long"); 				return 1;}
		read_blocks(point,1,(void*)&filler);
		memcpy(buffer, filler, length);
		n++;
	
	}
	
return saved;	
}
int sfs_fseek(int fileID, int loc){
	
	int n = loc/s_block.block;
	int i_node_index = directory[des_table[fileID].dir_index].i_index;
	if (n< 12){
		
		int point = i_table[i_node_index].pointers[n];
		des_table[fileID].writer_pointer = point*s_block.block + (loc % s_block.block);
		
		des_table[fileID].reader_pointer = point*s_block.block + (loc % s_block.block);
		 return 0;

	}
	else{
		n=n-12;
		int point = i_table[i_node_index].pointers[12];	
		int array[100];
		read_blocks( point, 1 , (void *)&array);
		point = array[n];
		point = point*s_block.block + (loc % s_block.block);	
		des_table[fileID].writer_pointer= point;
		des_table[fileID].reader_pointer = point; 
		return 0;

	}
}
int sfs_remove( char * name){
	int fileID = exists(name);
	int dir = root_exists2(name);
	int i_ID = directory[dir].i_index;

	 
	if( fileID ==-1 || dir ==-1 || i_ID ==-1) return 	-1 ;		
	if( fileID> 100){printf("out of bounds");return -1;}
	else{
			
			empty_des_entry.dir_index = -1;
			des_table[fileID] = empty_des_entry;
		
	}


	
	if( dir> 100){printf("out of bounds");return -1;}
	else{	
			empty_file.pointers[0]=-1;
			directory[dir] = empty_file;
			
		
	}
	int n = i_table[i_ID].size/ s_block.block;
	int i = 0;	
	for(i= 0; i<=n; i++){
		freeblocks[i_table[i_ID].pointers[i]].used = 0;
	} 
	if( n>=12){
		int point;
		int array[100];
		read_blocks( point, 1 , (void *)&array);
		n=n-11;
		i = 0;
		for( i = 0; i<=n; i++){
			freeblocks[array[i]].used = 0;
		}

	}
		 
	

	write_blocks(0,1,(void *)&s_block);
	write_blocks(1,1,(void *)&i_table);
	write_blocks(2,1,(void *)&directory);	
        write_blocks(499,1,(void *)&freeblocks);
return 0;


}
int exists( char *name){// returns the entry int the file descriptor place if exists or 100 else 
	int i = 0;
	while(1){
		if(i == 99){ return 100;}
		if(des_table[i].dir_index == -1) {i++;continue;}
		else{	
			if (strcmp(name,des_table[i].name)==0) return 					i; 	
			else i++; 
		}	
	}
}
int root_exists( char *name){// returns the  directory place if exists or 100 else 

	int i = 0;
	while(1){
		if(i == 99){ return 100;}
		if(directory[i].i_index == -1) {i++;continue;}
		else{	
			if (strcmp(name,directory[i].name)==0) return 					directory[i].i_index; 	
			else i++; 
		}	
	}
}
int root_exists2( char *name){// returns the  directory place if exists or 100 else 

	int i = 0;
	while(1){
		if(i == 99){ return 100;}
		if(directory[i].i_index == -1) {i++;continue;}
		else{	
			if (strcmp(name,directory[i].name)==0) return 					i; 	
			else i++; 
		}	
	}
}
int new_block(){

	int i = 0; 
	for (i= 0; i<499; i++){
		if (freeblocks[i].used ==0){
			freeblocks[i].used= 1;
			return i;
		}	
	}
	return -1;
	
}


