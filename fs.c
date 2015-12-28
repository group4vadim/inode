#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "fs.h"

const int size_of_block = SIZE_OF_BLOCK;
int filesystem_fd = -1;
const int number_of_root_block = NUMBER_OF_ROOT_BLOCK;


//загружаем данные из файла и инициализация глоб переменных
/*если файл не создан,то создаем файл с фс*/
int init()
{
	int res=0;
	/*попытка открыть существующий файл с фс, одновременно для чтения
	* и записи
	*/
	filesystem_fd=open(FILESYSTEM, O_RDWR, 0666);
	if(filesystem_fd<0)
	{
		//если не создан, создаем новый файл с фс
		filesystem_fd=open(FILESYSTEM, O_CREAT | O_RDWR, 0666);
		//создаем корень
		if (filesystem_fd<0||create_root()!=0)
			res=-1;
	}
	return res;//0-успешное завершение
}
 
//создаем корень
int create_root()
{
	int res=-1;
	inode_t *root=(inode_t *)create_block();
	if (root!=NULL)
	{
		//корневая директория
		root->status = BLOCK_STATUS_FOLDER;
		root->name[0] = '\0';
		root->stat.st_mode = S_IFDIR | 0777;
		root->stat.st_nlink = 2;
		if (write_block(number_of_root_block, root) == 0)
		{
			res = 0;
		}
		destroy_block(root);
	}
	return res;
}

void *create_block()
{
	//возвращает указатель на первый байт выделенной области
	return calloc(size_of_block, sizeof(char));
}

void destroy_block(void *block)
{
    free(block);//освобождаем память
}

int read_block(int number, void *block)
{
	int res=-1;
	//смещение от начала файла в байтах
	if (number>=0&&lseek(filesystem_fd, size_of_block * number, SEEK_SET) >= 0)
	{
		//cчитываем
		if (read(filesystem_fd,block,size_of_block)>=0)
		{
			res=0;
		}
	}
	return res;
}

int write_block(int number,void *block)
{
	int res = -1;
	if (number >= 0 && lseek(filesystem_fd, size_of_block * number, SEEK_SET) >= 0)
	{
		if (write(filesystem_fd, block, size_of_block) == size_of_block)
		{
			res = 0;
		}
	}
	return res;
}

//ищем первый свободный блок
int search_free_block()
{
	int number=number_of_root_block+1; //номер следования
	char status;
	int read_res;
	while(TRUE)
	{
		//выход за границы файла
		if (lseek(filesystem_fd,  size_of_block * number, SEEK_SET) < 0)
		{
			number = -1;
			break;
		}
		read_res=read(filesystem_fd,&status,sizeof(char));
		if (read_res < 0)
		{
			number = -1;
			break;
		}
		//конец файла или нашли свободный блок
		if (read_res == 0 || status == BLOCK_STATUS_FREE)
		{
			break;
		}
		//нашли номер этого блока
		number++;
	}
	return number;
}

//получаем блок по его номеру
void *get_block(int number)
{
	void *block = NULL;
	if (number >= 0)
	{
		//инициализируем блок
		block=create_block();
		if(block!=NULL&&read_block(number,block)!=0)
		{
			destroy_block(block);
			block=NULL;
		}
	}
	return block;
}

//заносим инфу о статусе
int set_block_status(int number, char status)
{
	int res=-1;
	if (number >= 0 && lseek(filesystem_fd, size_of_block * number + BLOCK_STATUS_OFFSET, SEEK_SET) >= 0)
	{
		//если запись без ошибок
		if (write(filesystem_fd, &status, sizeof(char)) == sizeof(char))
			res=0;
	}
	return res;
}

//удалить блок 
int remove_block(int number)
{
	int res=0;
	int status=get_block_status(number);
	//действия в зависимости от того,чем является блок
	switch(status) 
	{
		case BLOCK_STATUS_FREE:
			break;
		case BLOCK_STATUS_FOLDER:
			remove_folder(number);//удалить папку 
			break;
		case BLOCK_STATUS_FILE:
			remove_file(number);
			break;
		default:
			res= -1;
			break;
	}
	return res;
}

//поиск узла
//если найден,возвращает его номер
int search_inode(int node_number,char **node_names)
{
	int res=-1;
	//узел существует
	if (node_number >= 0 && node_names != NULL)
	{
		if (*node_names == NULL)//имя узла не задано
		{
			res= node_number;
		}
		else 
		{
			//ищем след свободный узел
			int next_node_number = search_inode_in_folder(node_number, *node_names);
			if (next_node_number > 0)
			{
				res = search_inode(next_node_number, node_names + 1);
			}
		}
	}
	return res;
}

//удаление файла
//то есть помечаем блок,где хранился файл как свободный
int remove_file(int number)
{
    return set_block_status(number, BLOCK_STATUS_FREE);
}

//удаление директории
int remove_folder(int number)
{
	int res=-1;
	inode_t *folder = (inode_t *)get_block(number);
	if (folder != NULL)//если папка не пуста
	{
		/*определяем начало и конец*/
		int *start = (int *)folder->content;
		int *end = (int *)((void *)folder + size_of_block);
		//проход по папке 
		while (start < end)
		{
			if (*start > 0)
			{
				remove_block(*start);
			}
			start++;
		}
		destroy_block(folder);//освобожиди память
		res = set_block_status(number, BLOCK_STATUS_FREE);
	}
	return res;
}

int create_folder(const char *name, mode_t mode)
{
	//ищем номер свободного блока
	int number = search_free_block();
	if (number >= 0)
	{
		//выделяем память
		inode_t *folder = (inode_t *)create_block();
		if (folder != NULL)
		{
			int name_size = strlen(name) + 1;
			if (name_size > NODE_NAME_MAX_SIZE)
			{
				name_size = NODE_NAME_MAX_SIZE;
			}
			//флаг,что блок является папкой
			folder->status = BLOCK_STATUS_FOLDER;
			memcpy(folder->name, name, name_size);
			folder->stat.st_mode = S_IFDIR | mode;
			folder->stat.st_nlink = 2;
			if (write_block(number, folder) != 0)
			{
				number = -1;
			}
			destroy_block(folder);//освобождаем память
			
		}
	}
	return number;
}

int create_file(const char *name, mode_t mode, dev_t dev)
{
	int number = search_free_block();
	if (number >= 0)
	{
		inode_t *file = (inode_t *)create_block();
		if (file != NULL)
		{
			int name_size = strlen(name) + 1;
			if (name_size > NODE_NAME_MAX_SIZE)
			{
				name_size = NODE_NAME_MAX_SIZE;
			}
			file->status = BLOCK_STATUS_FILE;
			memcpy(file->name, name, name_size);
			file->stat.st_mode = S_IFREG | mode;
			file->stat.st_rdev = dev;
			file->stat.st_nlink = 1;
			if (write_block(number, file) != 0)
			{
				number = -1;
			}
		destroy_block(file);
        }
    }
    return number;
}

//парсер адреса
char **split_path(const char *path)
{
	char **res = NULL;
	int path_size = strlen(path) + 1;
	char *copy_path = (char *)malloc(path_size);
	if (copy_path != NULL)
	{
		memcpy(copy_path, path, path_size);
		int depth = 0;//вложенность
		int i = 0;
		//пока не дошли до корневого
		while (copy_path[i] != '\0')
		{
			if (copy_path[i] == '/')
			{
				depth++;
				copy_path[i] = '\0';
			}
			i++;
			
		}
		if (copy_path[i - 1] == '\0')
		{
			depth--;
		}
		res = (char **)malloc(sizeof(char **) * (depth + 1));
		if (res != NULL)
		{
			i = 0;
			int j = 0;
			while (j < depth)
			{
				while (copy_path[i++] != '\0');
				res[j++] = create_name(copy_path + i);
			}
			res[j] = NULL;
		}
		free(copy_path);
	}
	return res;
}

int get_block_status(int number)
{
	int res=-1;
	if (number >= 0 && lseek(filesystem_fd, size_of_block * number + BLOCK_STATUS_OFFSET, SEEK_SET) >= 0)
	{
		char status;
		res= read(filesystem_fd, &status, sizeof(char));
		 if (res < 0)
		{
			res = -1;
		}
		else if (res == 0)//считано 0байт
		{
			res = BLOCK_STATUS_FREE;
		}
		else
		{
			res = status;//присваиваем полученное при считывании значение
			
		}
	}
	return res;
}

int get_inode_name(int number, char *buf)
{
	int res = -1;
	if (number >= 0 && lseek(filesystem_fd, size_of_block * number + NODE_NAME_OFFSET, SEEK_SET) >= 0)
	{
		//считываем в переменную и проверяем на ошибки
		if (read(filesystem_fd, buf, NODE_NAME_MAX_SIZE) == NODE_NAME_MAX_SIZE)
		{
			res = 0;
		}
	}
	return res;
}

int get_inode_stat(int number, stat_t *stbuf)
{
	int res = -1;
	if (number >= 0 && lseek(filesystem_fd, size_of_block * number + NODE_STAT_OFFSET, SEEK_SET) >= 0)
	{
		if (read(filesystem_fd, stbuf, sizeof(stat_t)) == sizeof(stat_t))
		{
			res = 0;
		}
	}
	return res;
}

//*buf-указатель на заданное имя
int set_inode_name(int number, char *buf)
{
	int res= -1;
	if (number >= 0 && lseek(filesystem_fd, size_of_block * number + NODE_NAME_OFFSET, SEEK_SET) >= 0)
	{
		if (write(filesystem_fd, buf, NODE_NAME_MAX_SIZE) == NODE_NAME_MAX_SIZE)
		{
			res = 0;
		}
	}
	return res;
}

int set_inode_stat(int number, stat_t *buf)
{
	int res = -1;
	if (number >= 0 && lseek(filesystem_fd, size_of_block * number + NODE_STAT_OFFSET, SEEK_SET) >= 0)
	{
		if (write(filesystem_fd, buf, sizeof(stat_t)) == sizeof(stat_t))
		{
			res = 0;
		}
	}
	return res;
}

char *create_name(const char *name)
{
	char *res = (char *)calloc(NODE_NAME_MAX_SIZE, sizeof(char));
	if (res != NULL)
	{
		int size = strlen(name) + 1;
		if (size > NODE_NAME_MAX_SIZE)
		{
			size = NODE_NAME_MAX_SIZE;
		}
		memcpy(res, name, size);
	}
	return res;
	
}

char *create_empty_name()
{
	return (char *)calloc(NODE_NAME_MAX_SIZE, sizeof(char));
}

void destroy_name(char *name)
{
    free(name);
}

// исключить имя последнего узла
char *exclude_last_node_name(char **node_names)
{
	char *result = NULL;
	if (node_names != NULL && *node_names != NULL)
	{
		 while (node_names[1] != NULL)
		{
			node_names++;
		}
		result = node_names[0];
		node_names[0] = NULL;
	}
	return result;
}

void destroy_node_names(char **node_names)
{
	if (node_names != NULL)
	{
		char **tmp = node_names;
		while (*tmp != NULL)
		{
			destroy_name(*tmp);
			tmp++;
		}
		free(node_names);
	}	
}

int clear_block(int number)
{
	int res = -1;
	if (number >= 0 && lseek(filesystem_fd, size_of_block * number, SEEK_SET) >= 0)
	{
		void *block = create_block();
		if (block != NULL)
		{
			if (write_block(number, block) == 0)
			{
				res = 0;
			}
			destroy_block(block);
		}
	}
	return res;
}

int add_inode_to_folder(int folder_number, int node_number)
{
	int res = -1;
	if (folder_number >= 0 && node_number > 0)
	{
		inode_t *folder = (inode_t *)get_block(folder_number);
		if (folder != NULL)
		{
			if (folder->status == BLOCK_STATUS_FOLDER)
			{
				int *start = (int *)folder->content;
				int *end = (int *)((void *)folder + size_of_block);
				//проход по папке
				while (start < end)
				{
					if (*start <= 0)
					{
						*start = node_number;
						break;
					}
					start++;
				}
				if (start < end)
				{
					res= write_block(folder_number, folder);
				}
			}
			destroy_block(folder);
			
		}
	}
	return res;
}

int remove_node_from_folder(int folder_number, int node_number)
{
	int res= -1;
	if (folder_number >= 0 && node_number > 0)
	{
		inode_t *folder = (inode_t *)get_block(folder_number);
		if (folder != NULL)
		{
			if (folder->status == BLOCK_STATUS_FOLDER)
			{
				int *start = (int *)folder->content;
				int *end = (int *)((void *)folder + size_of_block);
				while (start < end)
				{
					if (*start == node_number)
					{
						*start = 0;
						break;
					}
					start++;
				}
			if (start < end)
			{
				res = write_block(folder_number, folder);
			}
			else
			{
				res = 0;
			}
            }
            destroy_block(folder);
        }
    }
    return res;
}

int search_inode_in_folder(int folder_number, const char *node_name)
{
	int result = -1;
	if (folder_number >= 0 && node_name != NULL)
	{
		inode_t *folder = (inode_t *)get_block(folder_number);
		if (folder != NULL)
		{
			if (folder->status == BLOCK_STATUS_FOLDER)
			{
				char name[NODE_NAME_MAX_SIZE];
				int *start = (int *)folder->content;
				int *end = (int *)((void *)folder + size_of_block);
				while (start < end)
				{
					if (*start > 0 && get_inode_name(*start, name) == 0 && strcmp(node_name, name) == 0)
					{
						result = *start;
						break;
					}
					start++;
				}
			}
			destroy_block(folder);
		}
	}
	return result;
}