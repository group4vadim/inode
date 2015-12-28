#ifndef fs
#define fs
#define _FILE_OFFSET_BITS 64
#define FUSE_USE_VERSION 26
#include <fuse.h>
//структуры по одному байту
#pragma pack(1)
#ifdef DEBUG
#define TRACE printf("[DEBUG] FILE:%s LINE:%d\n", __FILE__, __LINE__);
#else

#define TRACE
#endif

#define FILESYSTEM "filesystem"
#define NUMBER_OF_ROOT_BLOCK 0 //номер корневого каталога
#define SIZE_OF_BLOCK 4096
#define NODE_NAME_MAX_SIZE 256 //макс размер имени
#define BLOCK_STATUS_OFFSET 0
#define NODE_NAME_OFFSET (BLOCK_STATUS_OFFSET + sizeof(char))
#define NODE_STAT_OFFSET (NODE_NAME_OFFSET + NODE_NAME_MAX_SIZE)
#define NODE_CONTENT_OFFSET (NODE_STAT_OFFSET + sizeof(stat_t))
#define NODE_CONTENT_MAX_SIZE (size_of_block - NODE_CONTENT_OFFSET)

//структура,хранящая информацию о файле
typedef struct stat stat_t;
//структура, определяющая файл или папка
typedef struct node inode_t;

enum boolean
{
    FALSE = 0,
    TRUE = 1,
};

struct node
{
    char status;
    char name[NODE_NAME_MAX_SIZE];
    stat_t stat;
    char content[0];
};

enum block_status
{
    BLOCK_STATUS_FREE = 0,
    BLOCK_STATUS_FOLDER = 1,
    BLOCK_STATUS_FILE = 2,
};

//загружаем данные из файла и инициализация глоб переменных
int init();
//создаем корневую
int create_root();
void *create_block();
int create_folder(const char *name, mode_t mode);
int create_file(const char *name, mode_t mode, dev_t dev);
// создать копию имени
char *create_name(const char *name);
// создать пустое имя
char *create_empty_name();


/*группа парсер*/
char **split_path(const char *path);
// исключить имя последнего узла
char *exclude_last_node_name(char **node_names);

/*группа чтения*/
int read_block(int number, void *block);

/*группа записи*/
int write_block(int number, void *block);

// удалить файл, метка свободный
int remove_file(int number);
// удалить папку
int remove_folder(int number);
// удалить блок
int remove_block(int number);
//  уничтожить блок
void destroy_block(void *block);
// освободить память
void destroy_name(char *name);
// освободить память
void destroy_node_names(char **node_names);
// стереть блок
int clear_block(int number);
// удалить узел из папки
int remove_node_from_folder(int folder_number, int node_number);

/*get группа*/
// получить блок
void *get_block(int number);
// получить состояние блока
int get_block_status(int number);
// получить имя узла
int get_inode_name(int number, char *buf);
// получить атрибуты узла
int get_inode_stat(int number, stat_t *stbuf);


/*set группа*/
// задать состояние блока
int set_block_status(int number, char status);
// задать имя узла
int set_inode_name(int number, char *buf);
// задать атрибуты узла
int set_inode_stat(int number, stat_t *buf);

// добавить узел в папку
int add_inode_to_folder(int folder_number, int node_number);

// искать первый свободный блок
int search_free_block();
// найти узел
int search_inode(int node_number, char **node_names);
// поиск узла в папке
int search_inode_in_folder(int folder_number, const char *node_name);

extern const int size_of_block;
extern int filesystem_fd;
extern const int number_of_root_block;

#endif