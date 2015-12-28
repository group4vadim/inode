#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "fs.h"

// получение атрибутов файла
int fs_getattr(const char *path, struct stat *stbuf);
// получение содержимого папки
int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
// определяем опции открытия файла
int fs_open(const char *path, struct fuse_file_info *fi);
// читаем данные из открытого файла
int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
// предоставляет возможность записать в открытый файл
int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
// создаём папку
int fs_mkdir(const char *path, mode_t mode);
// создаём файл
int fs_mknod(const char *path, mode_t mode, dev_t dev);
// переименование
int fs_rename(const char *old_path, const char *new_path);
// удалям папку
int fs_rmdir(const char *path);
// удаляем файл
int fs_unlink(const char *path);
// изменить размер файла
int fs_truncate(const char *path, off_t size);


/*
*функция определяет метаинформацию о файле (путь к нему -*path
*метаинформация возвращается в виде структуры stat).
*указатель на функцию передадим модулю фьюз как поле getattr cтруктуры 
*fuse_operations
*/
 
int fs_getattr(const char *path, struct stat *stbuf)
{
	int res = -ENOENT;
	char **node_names = split_path(path);//парсер имя узла
	if (node_names != NULL)
	{
		//определяем номер ущла
		int number = search_inode(number_of_root_block, node_names);
		//записываем инфу об узле в stbuf 
		if (number >= 0 && get_inode_stat(number, stbuf) == 0)
		{
			res = 0;
		}
		destroy_node_names(node_names);// освобождение памяти
	}
	return res;
}

/*определяет порядок чтения данных из директории, указатель не нее отдадим в качестве поля readdir*/
/*т.е получаем содержимое каталога, используя указатель на функцию filler*
* вызывается при попытке просмотра содержимого(н-р ls)
*/
int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	int res = -ENOENT;
	char **node_names = split_path(path);
	if (node_names != NULL)
	{
		//определеяем номер блока с данным узлом
		int number = search_inode(number_of_root_block, node_names);
		if (number >= 0)
		{
			//получаем блок с папкой
			inode_t *folder = (inode_t *)get_block(number);
			 if (folder != NULL)
			{
				if (folder->status == BLOCK_STATUS_FOLDER)
				{
					res = 0;
					filler(buf, ".", NULL, 0);
					filler(buf, "..", NULL, 0);
					char name[NODE_NAME_MAX_SIZE];
					stat_t stat;
					int *start = (int *)folder->content;
					int *end = (int *)((void *)folder + size_of_block);
					//проход по всей папке
					while(start<end)
					{
						//считываем имя и статут узла в переменны
						//если функции выполнились успешно
						 if (*start > 0 && get_inode_name(*start, name) == 0 && get_inode_stat(*start, &stat) == 0)
						 {
							  if (filler(buf, name, &stat, 0) != 0)
							  {
								  break;
							  }
						 }
						 start++;
					}
				}
				destroy_block(folder);
			}
		}
		destroy_node_names(node_names);
	}
	return res;
}
 
//определяет имеет ли право пользователь открыть файл /hello, реализуется через анализ данных структуры типа fuse_file_info
int fs_open(const char *path, struct fuse_file_info *fi)
{
    return 0;
}
 /*определяет, как именно будет считываться информация из файла для передачи пользователю*/
/*функция чтения данных из открытого файла
*возвращает столько байтов,сколько было запрошено,иначе ошибка
*/
int fs_read(const char *path, char *buf, size_t size, off_t offset,struct fuse_file_info *fi)
{
	int res=-ENOENT;
	//идет ряд операций для получения блока с файлом по заданному адресу
	char **node_names = split_path(path);
	if (node_names != NULL)
	{
		int number = search_inode(number_of_root_block, node_names);
		{
			if (number >= 0)
			{
				inode_t *file = (inode_t *)get_block(number);
				if (file != NULL)
				{
					if (file->status == BLOCK_STATUS_FILE)
					{
						if (offset < NODE_CONTENT_MAX_SIZE)
						{
							if (offset + size > NODE_CONTENT_MAX_SIZE)
							{
								size = NODE_CONTENT_MAX_SIZE - offset;
							}
							memcpy(buf, file->content + offset, size);
							res=size;//выполнилось без ошибки
						}
						else {
							res=0;
						}
					}
					destroy_block(file);
				}
			}
			 destroy_node_names(node_names);
		}
	}
	return res;
}

/*
* запись данных в открытый файл, возвращает кол-во записанных байтов, должно ра
передан. кол-ву, иначе ошибка.
*/
int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	int res = -ENOENT;
	//ряд операций для получения блока с заданным адресом
	char **node_names = split_path(path);
	if (node_names != NULL)
	{
		int number = search_inode(number_of_root_block, node_names);
		if (number >= 0)
		{
			inode_t *file = (inode_t *)get_block(number);
			if (file != NULL)
			{
				if (file->status == BLOCK_STATUS_FILE)
				{
					if (offset < NODE_CONTENT_MAX_SIZE)
					{
						if (offset + size > NODE_CONTENT_MAX_SIZE)
						{
							 size = NODE_CONTENT_MAX_SIZE - offset;
						}
						memcpy(file->content + offset, buf, size);
						if (file->stat.st_size < offset + size)
						{
							 file->stat.st_size = offset + size;
						}
						if (write_block(number, file) == 0)
						{
							res=size;
						}
					}
					else 
					{
						res=0;
					}
				}
				destroy_block(file);
			}
		}
		destroy_node_names(node_names);
	}
	return res;
}

/*создаем директорий*/
int fs_mkdir(const char *path, mode_t mode)
{
	int res=-ENOENT;
	//получаем блок с заданным адресом
	char **node_names = split_path(path);
	if (node_names != NULL)
	{
		//без имени последнего узла
		char *name = exclude_last_node_name(node_names);
		if (name != NULL)
		{
			//определяем номер узла.
			int folder_number = search_inode(number_of_root_block, node_names);
			if (folder_number >= 0)
			{
				//создаем папку
				int new_folder = create_folder(name, mode);
				if (new_folder >= 0 && add_inode_to_folder(folder_number, new_folder) == 0)
				{
					//добавили узел в папку по номеру папки и номеру узла
					res = 0;
				}
			}
			 destroy_name(name);
		}
		destroy_node_names(node_names);
	}
	return res;
}

/*
*Создает узел file.будет вызываться для создания узлов, 
* отличных от каталога и символических ссылок.
*/
int fs_mknod(const char *path, mode_t mode, dev_t dev)
{
	int res = -ENOENT;
	char **node_names = split_path(path);
	if (node_names != NULL)
	{
		char *name = exclude_last_node_name(node_names);
		if (name != NULL)
		{
			int folder_number = search_inode(number_of_root_block, node_names);
			if (folder_number >= 0)
			{
				//создаем узел file
				int new_file = create_file(name, mode, dev);
				if (new_file >= 0 && add_inode_to_folder(folder_number, new_file) == 0)
				{
					res=0;
				}
			}
			destroy_name(name);
		}
		destroy_node_names(node_names);
	}
	return res;
}

/*
*переименовывание файла
*т.е по сути переименовывание-это изменение адреса
*/
int fs_rename(const char *old_path, const char *new_path)
{
	int res = -ENOENT;
	char **old_node_names = split_path(old_path);
	if (old_node_names != NULL)
	{
		 char **new_node_names = split_path(new_path);
		if (new_node_names != NULL)
		{
			char *old_name = exclude_last_node_name(old_node_names);
			if (old_name != NULL)
			{
				char *new_name = exclude_last_node_name(new_node_names);
				if (new_name != NULL)
				{
					int old_folder_number = search_inode(number_of_root_block, old_node_names);
					int new_folder_number = search_inode(number_of_root_block, new_node_names);
					int node_number = search_inode_in_folder(old_folder_number, old_name);
					remove_node_from_folder(old_folder_number, node_number);
					add_inode_to_folder(new_folder_number, node_number);
					set_inode_name(node_number, new_name);
					res= 0;
					destroy_name(new_name);
				}
				destroy_name(old_name);
			}
			destroy_node_names(new_node_names);
		}
		destroy_node_names(old_node_names);
	}
	return res;
}

//удаление директории
int fs_rmdir(const char *path)
{
	int res = -ENOENT;
	char **node_names = split_path(path);
	if (node_names != NULL)
	{
		char *name = exclude_last_node_name(node_names);
		if (name != NULL)
		{
			int folder_number = search_inode(number_of_root_block, node_names);
			if (folder_number >= 0)
			{
				int node_number = search_inode_in_folder(folder_number, name);
				if (node_number >= 0)
				{
					if (remove_node_from_folder(folder_number, node_number) == 0 && remove_block(node_number) == 0)
					{
						res=0;
					}
					
				}
			}
			destroy_name(name);
		}
		destroy_node_names(node_names);
	}
	return res;
}

// удаление файла
int fs_unlink(const char *path)
{
	int res = -ENOENT;
	char **node_names = split_path(path);
	if (node_names != NULL)
	{
		char *name = exclude_last_node_name(node_names);
		if (name != NULL)
		{
			int folder_number = search_inode(number_of_root_block, node_names);
			if (folder_number >= 0)
			{
				int node_number = search_inode_in_folder(folder_number, name);
				if (node_number >= 0)
				{
					if (remove_node_from_folder(folder_number, node_number) == 0 && remove_block(node_number) == 0)
					{
						res=0;
					}
				}
			}
			destroy_name(name);
		}
		destroy_node_names(node_names);
	}
	return res;
}

//изменение размера файла
int fs_truncate(const char *path, off_t size)
{
	int res = -ENOENT;
	char **node_names = split_path(path);
	if (node_names != NULL)
	{
		int number = search_inode(number_of_root_block, node_names);
		if (number >= 0)
		{
			stat_t stat;
			if (get_inode_stat(number, &stat) == 0)
			{
				if (size <= NODE_CONTENT_MAX_SIZE)
				{
					stat.st_size = size;
					if (set_inode_stat(number, &stat) == 0)
					{
						res=0;
					}
				}
			}
		}
		destroy_node_names(node_names);
	}
	return res;
}


struct fuse_operations fs_oper= 
{
	.getattr    = fs_getattr,
	.readdir    = fs_readdir,
	.open       = fs_open,
	.read       = fs_read,
	.write      = fs_write,
	.mkdir      = fs_mkdir,
	.mknod      = fs_mknod,
	.rename     = fs_rename,
	.rmdir      = fs_rmdir,
	.unlink     = fs_unlink,
	.truncate   = fs_truncate,
};	//необходимая для создания файловой системы переменная структуры с типом fuse_operations, будет необходимо передать ее в функцию fuse_main

/*структура fuse_operations передает указатель на функции, 
* которые будут вызываться для выполнения 
соответ. действия */

/*то есть создаем небходимые функции с логикой их выполнения,
* затем создаем переменную fuse_operations и отдаем ей 
* соотв. функций,которые необходимо будет использовать*/
int main(int argc, char *argv[])
{
    if (init() != 0)
    {
        printf("initialization error\n");
        return -1;
    }
    return fuse_main(argc, argv, &fs_oper, NULL);
}