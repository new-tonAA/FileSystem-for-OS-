//代码写了两天两夜，给个好评吧老师，谢谢老师


//文件分为文件夹和文件，其中两者都有目录，目录被单独拿来作为文件主要信息，文件主要内容存储在文件数据块本身
//初始化后会有：
//0块为MBR
//1块为空闲位置位图
//2块用来放根目录
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <ctime>
#include <cstring>
#include <sstream>
#include <algorithm>

using namespace std;
static const int DISK_SIZE=8*1024*1024;//硬盘大小固定8MB
static const int BLOCK_SIZE=1024;//块大小
static const int BLOCK_COUNT=DISK_SIZE/BLOCK_SIZE;//块数量
static const char*DISK_FILE="Disk.bin";//模拟磁盘的二进制文件

struct DirEntryDisk;
struct FileDataBlock;

//目录项结构
struct DirEntryDisk 
{
    char name[12];//名称——12字节
    bool isDir;//是否目录（文件夹）——1字节
    int firstBlock;//目录块号或文件数据首块号——4字节
    int size;//文件大小，目录为0——4字节
    time_t createTime;//创建时间
    int nextEntryBlock;//同级链表中下一个目录项块号，-1无——4字节
    int childEntryBlock;//目录的子目录或文件链表首块号，文件为-1——4字节
};

//文件数据块结构
struct FileDataBlock 
{
    int nextBlock;//下一个数据块号，-1结尾
    char data[BLOCK_SIZE-sizeof(int)];
};

vector<bool>bitmap(BLOCK_COUNT, false);//空闲位图

//函数声明：
void saveBitmap();//保存位图到磁盘（块1）
void loadBitmap();//从磁盘加载位图
int allocBlock();//分配一个空闲块
void freeBlock(int block);//释放块
void writeDirEntry(int blockId, const DirEntryDisk&entry);//写目录项到指定块
DirEntryDisk readDirEntry(int blockId);//读目录项，返回的是目录类（结构体）
void Format();//格式化
string timeStr(time_t t);//转化成可以读的时间
int FreeSpace();//有多少空闲位置
vector<string> splitPath(const string& path);//分割路径字符串
int findDirBlock(const string& path);//通过路径找到目录块号（目录才返回块号，找不到返回-1）
int findEntryBlock(const string& path);//通过路径找到目录项块号（目录或文件），返回块号，不存在返回-1
int findParentDirBlock(const string &path,string &name);//找父目录块和目标名
void printTreeRec(int blockId,int depth);//递归打印目录树，与二叉树一致
void Tree(const string& path);//打印
bool CreateDirRec(const vector<string>& parts,int index,int curBlock);//多级目录创建递归辅助
void CreateDir(const string& path);
void freeFileDataBlocks(int startBlock);//释放文件所有数据块，用于删除文件
void deleteDirRec(int blockId);//递归删除目录
void DeleteDir(const string&path);//删除目录（递归删除）
void CreateFile(const string& path);//创建文件（创建目录项和数据块）
void DeleteFile(const string& path);//删除文件
void WriteFile(const string& path, const string& content);//写文件内容（覆盖写入，传入字符串）
bool ReadRealFile(const string& realPath, string& outContent);//读取真实磁盘文件内容到字符串（包含二进制数据）
void CopyFileCmd(const string& arg);//增加CopyFile命令处理
void ReadFile(const string& path);//读文件内容（输出到控制台）

void saveBitmap()//保存位图到磁盘（块1）
{
    fstream fs(DISK_FILE,ios::binary|ios::in|ios::out);//打开磁盘
    fs.seekp(1*BLOCK_SIZE,ios::beg);//找到第一块
    for (int i=0;i<BLOCK_COUNT;i+=8)//以char为单位，写入每一位（即每一次写入8位）
	{
        unsigned char byte=0;//待写入的8位
        for(int j=0;j<8&&i+j<BLOCK_COUNT;++j)//i+j表示的是块号
            if(bitmap[i+j])
				byte|=(1<<j);//写入，通过1的移位方式，将这个字符的第j个位置为1
        fs.write(reinterpret_cast<char*>(&byte),sizeof(byte));//将8位写入文件
    }
    fs.close();//关闭文件，成功写入
}

void loadBitmap()//从磁盘加载位图
{
    fstream fs(DISK_FILE,ios::binary|ios::in);//打开磁盘
    if(!fs) 
		return;//文件不存在时跳过
    fs.seekg(1*BLOCK_SIZE,ios::beg);//从位图开始读
    for(int i=0;i<BLOCK_COUNT;i+=8)//一位一位读
	{
        unsigned char byte=0;
        fs.read(reinterpret_cast<char*>(&byte),sizeof(byte));
        for(int j=0;j<8&&i+j<BLOCK_COUNT;++j)
            bitmap[i+j]=(byte&(1<<j))!=0;//处理并存
    }
    fs.close();
}

int allocBlock()//分配一个空闲块
{
    for(int i=3;i<BLOCK_COUNT;++i) 
	{  // 0-2保留
        if(!bitmap[i])//如果是空的
		{
            bitmap[i]=true;//将这块改成非空
            saveBitmap();//同时更新磁盘
            return i;//返回块号
        }
    }
    return -1;//没找到
}

void freeBlock(int block)//释放块
{
    if(block<0||block>=BLOCK_COUNT)//如果块号非法
		return;//忽略
    bitmap[block]=false;//将位图更改
    saveBitmap();//同步更新磁盘的位图
}

void writeDirEntry(int blockId, const DirEntryDisk&entry)//写目录项到指定块
{
    fstream fs(DISK_FILE, ios::binary|ios::in|ios::out);//打开文件
    fs.seekp(blockId*BLOCK_SIZE,ios::beg);//找到目标
    fs.write(reinterpret_cast<const char*>(&entry), sizeof(DirEntryDisk));//写
    fs.close();//关
}

DirEntryDisk readDirEntry(int blockId)//读目录项，返回的是目录类（结构体）
{
    DirEntryDisk entry;
    fstream fs(DISK_FILE,ios::binary|ios::in);//打开磁盘
    fs.seekg(blockId*BLOCK_SIZE, ios::beg);//找
    fs.read(reinterpret_cast<char*>(&entry), sizeof(DirEntryDisk));//读
    fs.close();//关
    return entry;//返回结果
}


void Format()//格式化
{
	cout<<"is formating, please wait..."<<endl;
    ofstream ofs(DISK_FILE,ios::binary|ios::trunc);//打开磁盘
    vector<char> zeros(BLOCK_SIZE,0);//搞一个块的0
    for(int i=0;i<BLOCK_COUNT;i++)//将每个块写0
		ofs.write(zeros.data(),BLOCK_SIZE);
    ofs.close();//关磁盘

    bitmap.assign(BLOCK_COUNT, false);//格式化位图
    bitmap[0]=bitmap[1]=bitmap[2]=true;//更新位图

    DirEntryDisk root{};//根目录
    strcpy(root.name, "/");//根目录名字为/
    root.isDir=true;//是目录类似文件
    root.firstBlock=2;//所在块号
    root.size=0;//目录，文件大小为0
    root.createTime=time(nullptr);//获得创建时间
    root.nextEntryBlock=-1;//没有下一个
    root.childEntryBlock=-1;//没有下一个
    writeDirEntry(2,root);//写进磁盘

    saveBitmap();//同步更新磁盘的位图，保持一致
    cout<<"Format complete."<<endl;
}

string timeStr(time_t t)//转化成可以读的时间
{
    char buf[64];
    struct tm* tm_info=localtime(&t);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm_info);
    return string(buf);
}

int FreeSpace()//有多少空闲位置
{
    return count(bitmap.begin(),bitmap.end(),false);
}

vector<string> splitPath(const string& path)//分割路径字符串
{
    vector<string> parts;
    stringstream ss(path);//全部的读入
    string item;//用来装分割结果
    while(getline(ss,item,'/'))
        if (!item.empty())
			parts.push_back(item);
    return parts;
}

//用类似数据结构的树状形式存储
int findDirBlock(const string& path)//通过路径找到目录块号（目录才返回块号，找不到返回-1）
{
    if (path.empty()||path[0]!='/') //无效路径
		return -1;
    if (path=="/")//根目录
		return 2;

    vector<string> parts=splitPath(path);//获得多级目录，但是永远是从根目录开始的
    int curBlock=2;//从根目录开始
    for(const string& name:parts)//遍历向量
	{
        DirEntryDisk curDir=readDirEntry(curBlock);//获得当前目录
        int child=curDir.childEntryBlock;//获得当前目录下的孩子
        bool found=false;
        while(child>=0)//如果有孩子
		{
            DirEntryDisk e=readDirEntry(child);//则去拿孩子
            if(e.isDir&&name==e.name)//如果孩子是目录并且名字相同
			{
                curBlock=child;//则获取孩子
                found=true;//找到了
                break;
            }
            child=e.nextEntryBlock;//如果孩子不是目标，则找另一个孩子
        }
        if(!found)//如果没找到
			return -1;
    }
    return curBlock;//找到了就返回目标块号
}


int findEntryBlock(const string& path)//通过路径找到目录项块号（目录或文件），返回块号，不存在返回-1
{
    if(path.empty()||path[0]!='/')//非法
		return-1;
    if(path=="/")//根
		return 2;

    vector<string> parts=splitPath(path);
    int curBlock=2;//从根开始
    for(size_t i=0;i<parts.size();i++)//遍历路径
	{
        DirEntryDisk curDir=readDirEntry(curBlock);
        int child=curDir.childEntryBlock;
        bool found=false;
        while(child>=0)
		{
            DirEntryDisk e=readDirEntry(child);
            if (parts[i]==e.name) 
			{
                if(i==parts.size()-1) 
					return child; //最后一个部分，返回对应块号
                if(!e.isDir)//不是目录
					return -1; //中间部分必须是目录
                curBlock=child;
                found=true;
                break;
            }
            child=e.nextEntryBlock;
        }
        if(!found)//没找到
			return -1;
    }
    return -1;
}

int findParentDirBlock(const string &path,string &name)//找父目录块和目标名
{
    if(path.empty()||path[0]!='/')//非法
		return -1;
    else if(path=="/")//根
		return -1;

    size_t pos=path.find_last_of('/');//最后一次出现/的位置
    if(pos==string::npos||pos==0)//如果上一级就是根目录
	{
        name=path.substr(1);//提取文件名
        return 2;//返回根目录地址
    }
    name=path.substr(pos+1);//提取文件名
    string parentPath=path.substr(0, pos);//自己的路径去掉自己，即是父
    if(parentPath.empty())//打补丁，如果没了
		parentPath="/";//就是根
    return findDirBlock(parentPath);//从根开始根据路径找父
}

void printTreeRec(int blockId,int depth)//递归打印目录树，与二叉树一致
{
    if(blockId<0)//非法
		return;
    DirEntryDisk entry=readDirEntry(blockId);//找目标目录
    for(int i=0;i<depth;++i)
		cout<<"  ";
    cout<<(entry.isDir?"[D] ":"[F] ")<<entry.name;
    if(!entry.isDir)//如果是文件
		cout<<" ("<<entry.size<<" bytes)";//还需要打印文件大小
	cout<<" "<<timeStr(entry.createTime);
    cout<<endl;
    printTreeRec(entry.childEntryBlock,depth+1);//打印孩子（深度+1）
    if(depth>0)//根的兄弟就别打了
    	printTreeRec(entry.nextEntryBlock,depth);//打印兄弟
}

void Tree(const string& path)//打印
{
    int block=findDirBlock(path);//找到目标块号
    if(block<0)//非法
    {
        cout<<"path not found: "<<path<<endl;
        return;
    }
    printTreeRec(block, 0);//打印
}

//question
bool CreateDirRec(const vector<string>& parts,int index,int curBlock)//多级目录创建递归辅助
{
    if(index>=(int)parts.size())
		return true; 

    DirEntryDisk curDir=readDirEntry(curBlock);//获得目录
    int child=curDir.childEntryBlock;//获得孩子块号
    int prevChild=-1;//现在孩子

    while(child>=0)//如果有孩子，也就是直到没有孩子才可以创建新，否则会重复创建
	{
        DirEntryDisk e=readDirEntry(child);//获得孩子
        if(e.isDir&&string(e.name)==parts[index])//如果孩子是目录，且名字同路径中一个一样
            return CreateDirRec(parts,index+1,child);//继续深挖
        prevChild=child;//现在的孩子是孩子
        child=e.nextEntryBlock;//下一个孩子
    }

    int newBlock=allocBlock();//找到新的空闲位置
    if(newBlock==-1) 
	{
        cout<<"No space to allocate directory."<<endl;
        return false;
    }
    DirEntryDisk newDir{};//创建新
    strncpy(newDir.name,parts[index].c_str(),sizeof(newDir.name)-1);//得到新名字
    newDir.isDir=true;//是目录
    newDir.firstBlock=newBlock;//位置
    newDir.size=0;//大小为0
    newDir.createTime=time(nullptr);//创建时间
    newDir.nextEntryBlock=-1;//新的，没有下一个
    newDir.childEntryBlock=-1;//新的，没有孩子
    writeDirEntry(newBlock, newDir);//更新文件

    if(prevChild<0)//如果现在没孩子
        curDir.childEntryBlock=newBlock;//那就有孩子了，新的就是你的孩子
    else//如果这一级有孩子
	{
        DirEntryDisk prevEntry=readDirEntry(prevChild);//读孩子
        prevEntry.nextEntryBlock=newBlock;//让新的做孩子的兄弟
        writeDirEntry(prevChild, prevEntry);//更新文件
    }
    writeDirEntry(curBlock,curDir);//更新文件

    return CreateDirRec(parts,index+1,newBlock);//继续
}

void CreateDir(const string& path)
{
    if(path.empty()||path[0]!='/')//非法
    {
        cout<<"Invalid path."<<endl;
        return;
    }
    if(path=="/")//根
	{
        cout<<"Root directory already exists."<<endl;//有根了
        return;
    }
    int dirBlock=findDirBlock(path);//找路径
    if(dirBlock>=0)//如果本来就存在
	{
        cout<<"Directory already exists."<<endl;
        return;
    }
    vector<string> parts=splitPath(path);
    if(CreateDirRec(parts,0,2))//从根开始创建
        cout<<"CreateDir success: "<<path<<endl;
}


void freeFileDataBlocks(int startBlock)//释放文件所有数据块，用于删除文件
{
    int cur=startBlock;
    while(cur!=-1&&cur>=3)//如果合法
	{
        fstream fs(DISK_FILE, ios::binary|ios::in|ios::out);//打开磁盘
        fs.seekg(cur*BLOCK_SIZE,ios::beg);//找到位置
        int nextBlock;
        fs.read(reinterpret_cast<char*>(&nextBlock), sizeof(int));//找到下一个块
        fs.close();
        freeBlock(cur);//更改位图
        cur=nextBlock;//继续弄下一个
    }
}

void deleteDirRec(int blockId)//递归删除目录
{
    if(blockId<0)//非法
		return;
    DirEntryDisk entry=readDirEntry(blockId);//找到块
    int child=entry.childEntryBlock;//找到孩子

    while(child>=0)//如果有孩子
	{
        DirEntryDisk childEntry=readDirEntry(child);//读孩子
        int nextChild=childEntry.nextEntryBlock;//孩子的兄弟
        if (childEntry.isDir)//如果孩子是目录
            deleteDirRec(child);
        else//如果孩子是文件
		{
            freeFileDataBlocks(childEntry.firstBlock);
            freeBlock(child);//还要删除文件内容块
        }
        child=nextChild;//兄弟（另一个孩子）
    }
    freeBlock(blockId);//把自己也删
}

void DeleteDir(const string&path)//删除目录（递归删除）
{
    if(path=="/"||path.empty())//不能让用户乱来
	{
        cout<<"Cannot delete root directory."<<endl;
        return;
    }
    int block=findDirBlock(path);
    if(block<0)//非法
	{
        cout << "Directory not found: " << path << endl;
        return;
    }

    string name;
    int parentBlock=findParentDirBlock(path, name);//获取上级目录和名字
    if (parentBlock<0)//非法
	{
        cout << "Parent directory not found." << endl;
        return;
    }

    DirEntryDisk parent=readDirEntry(parentBlock);//找到上级目录
    int prev=-1;
    int cur=parent.childEntryBlock;//找到父目第一个孩子
    while(cur>=0)//当父母真的有孩子
	{
        DirEntryDisk e=readDirEntry(cur);//获得
        if(string(e.name)==name)//如果就是自己
		{
            if(prev<0)//如果自己就是父母的第一个孩子
			{
                parent.childEntryBlock=e.nextEntryBlock;//自己挂了让自己的兄弟当大哥
                writeDirEntry(parentBlock, parent);//更新
            } 
			else//不能直接删除
			{
                DirEntryDisk prevEntry=readDirEntry(prev);//找到自己走过的上一个位置
                prevEntry.nextEntryBlock=e.nextEntryBlock;//自己挂了让兄弟当二哥
                writeDirEntry(prev,prevEntry);//更新
            }
            break;
        }
        prev=cur;//获取现在位置
        cur=e.nextEntryBlock;//自己还没找到，继续找
    }

    deleteDirRec(block);//删了
    cout<<"DeleteDir success: "<<path<<endl;
}


void CreateFile(const string& path)//创建文件（创建目录项和数据块）
{
    string name;
    int parentBlock=findParentDirBlock(path, name);//找父母
    if (parentBlock<0)//没父母是非法的
	{
        cout<<"Parent directory not found."<<endl;
        return;
    }

    int existBlock=findEntryBlock(path);//检查文件是否已存在
    if (existBlock>=0) 
	{
        cout<<"File or directory already exists." <<endl;
        return;
    }

    int dataBlock=allocBlock();//获得空闲位置放文件本身
    if(dataBlock==-1)
	{
        cout<<"No space to allocate file data block." <<endl;
        return;
    }

    int fileEntryBlock=allocBlock();//获得空闲位置放目录
    if(fileEntryBlock==-1)
	{
        freeBlock(dataBlock);
        cout<<"No space to allocate file entry block."<<endl;
        return;
    }

    DirEntryDisk parent=readDirEntry(parentBlock);//获得父目录

    //新目录项初始化
    DirEntryDisk fileEntry{};
    strncpy(fileEntry.name, name.c_str(),sizeof(fileEntry.name)-1);//名字
    fileEntry.isDir=false;//是文件
    fileEntry.firstBlock=dataBlock;//数据地址
    fileEntry.size=0;//暂时为0
    fileEntry.createTime=time(nullptr);//创建时间
    fileEntry.nextEntryBlock=-1;//刚出生没弟弟
    fileEntry.childEntryBlock=-1;//刚出生没儿子

    if(parent.childEntryBlock<0)//如果父母就没孩子，自己一出生就是老大
        parent.childEntryBlock=fileEntryBlock;
    else//自己不是老大，则要找最小的大哥
	{
        int last=parent.childEntryBlock;
        while(true)
		{
            DirEntryDisk lastEntry=readDirEntry(last);
            if(lastEntry.nextEntryBlock<0)//找到最小的哥哥了
				break;
            last=lastEntry.nextEntryBlock;//没找到就继续找
        }
        DirEntryDisk lastEntry=readDirEntry(last);//最小的哥哥
        lastEntry.nextEntryBlock=fileEntryBlock;//当哥哥的弟弟
        writeDirEntry(last,lastEntry);//更新哥哥
    }
    writeDirEntry(parentBlock, parent);//更新父母
    writeDirEntry(fileEntryBlock, fileEntry);//更新自己

    // 初始化文件数据块为空链表
    FileDataBlock emptyBlock{};
    emptyBlock.nextBlock=-1;
    fstream fs(DISK_FILE, ios::binary|ios::in|ios::out);//打开磁盘
    fs.seekp(dataBlock*BLOCK_SIZE,ios::beg);//找位置
    fs.write(reinterpret_cast<const char*>(&emptyBlock), sizeof(FileDataBlock));//写
    fs.close();

    cout<<"CreateFile success: "<<path<<endl;
}


void DeleteFile(const string& path)//删除文件
{
    string name;
    int parentBlock=findParentDirBlock(path,name);//找父母
    if(parentBlock<0)
	{
        cout<<"Parent directory not found."<<endl;
        return;
    }

    int fileBlock=findEntryBlock(path);//找文件目录在哪
    if(fileBlock<0) 
	{
        cout<<"File not found."<<endl;
        return;
    }

    DirEntryDisk fileEntry=readDirEntry(fileBlock);//获得文件目录
    if(fileEntry.isDir)//如果不是文件而是文件夹
	{
        cout<<"Path is a directory, not a file."<<endl;
        return;
    }

    DirEntryDisk parent=readDirEntry(parentBlock);//获得父母
    int prev=-1;
    int cur=parent.childEntryBlock;//获得父母的孩子
    while(cur>=0)//当父母有孩子
	{
        DirEntryDisk e=readDirEntry(cur);//获得这个孩子
        if(string(e.name)==name)//如果是自己
		{
            if(prev<0)//如果自己是大哥
			{
                parent.childEntryBlock = e.nextEntryBlock;
                writeDirEntry(parentBlock, parent);
            } 
			else//如果自己不是大哥
			{
                DirEntryDisk prevEntry=readDirEntry(prev);//获得最小的哥哥
                prevEntry.nextEntryBlock=e.nextEntryBlock;//我挂了，我的哥哥和弟弟连在一起
                writeDirEntry(prev, prevEntry);//更新哥哥
            }
            break;
        }
        prev=cur;//下一个
        cur=e.nextEntryBlock;
    }

    freeFileDataBlocks(fileEntry.firstBlock);//释放文件内容块
    freeBlock(fileBlock);//释放文件目录块
    cout<<"DeleteFile success: "<<path<<endl;
}


void WriteFile(const string& path, const string& content)//写文件内容（覆盖写入，传入字符串）
{
    int fileBlock=findEntryBlock(path);//找到文件目录位置
    if (fileBlock<0)
	{
        cout<<"File not found."<<endl;
        return;
    }
    DirEntryDisk fileEntry=readDirEntry(fileBlock);//获得文件目录
    if(fileEntry.isDir)
	{
        cout<<"Path is a directory, not a file."<<endl;
        return;
    }

    freeFileDataBlocks(fileEntry.firstBlock);//将原来的文件内容删除

    //分配数据块链写入
    const char* ptr=content.c_str();
    size_t left=content.size();
    int firstBlock=-1;
    int prevBlock=-1;

    fstream fs(DISK_FILE,ios::binary|ios::in|ios::out);//打开磁盘
    while(left>0)
	{
        int blockId=allocBlock();//分配位置给他写
        if(blockId==-1)
		{
            cout<<"No space to write file content."<<endl;
            //如果没有位置，就释放已分配块
            int cur=firstBlock;
            while(cur!=-1)
			{
                fs.seekg(cur*BLOCK_SIZE,ios::beg);
                int nb;
                fs.read(reinterpret_cast<char*>(&nb), sizeof(int));
                freeBlock(cur);
                cur=nb;
            }
            fs.close();
            return;
        }

        FileDataBlock datablock{};
        datablock.nextBlock=-1;//后面没有了
        size_t writeLen=min(left, sizeof(datablock.data));//获得大小
        memcpy(datablock.data, ptr, writeLen);//获得文件大小

        fs.seekp(blockId*BLOCK_SIZE,ios::beg);//找到目录
        fs.write(reinterpret_cast<const char*>(&datablock), sizeof(FileDataBlock));//更新目录记录文件大小

        if(prevBlock!=-1)//后面没东西了
		{
            // 更新上一个块nextBlock指向本块
            fs.seekp(prevBlock*BLOCK_SIZE, ios::beg);//找位置
            int nextBlockId=blockId;
            fs.write(reinterpret_cast<const char*>(&nextBlockId), sizeof(int));//写
        }
		else//后面还有东西，搞个新块继续写
            firstBlock=blockId;
        prevBlock=blockId;//更新下一个块
        ptr+=writeLen;//更新长度
        left-=writeLen;
    }
    fs.close();
    fileEntry.firstBlock=firstBlock;//获得文件位置
    fileEntry.size=(int)content.size();//获得文件大小
    writeDirEntry(fileBlock, fileEntry);//写入磁盘
    cout<<"WriteFile success: "<<path<<endl;
}

bool ReadRealFile(const string& realPath, string& outContent)//读取真实磁盘文件内容到字符串（包含二进制数据）
{
    ifstream ifs(realPath,ios::binary);//打开真实文件
    if(!ifs)//如果不存在这个真实文件
		return false;
    ifs.seekg(0,ios::end);//走到结尾
    size_t size=ifs.tellg();//获得文件大小
    ifs.seekg(0,ios::beg);//从头开始
    outContent.resize(size);//更改大小
    ifs.read(&outContent[0],size);//获得内容
    return true;
}

void CopyFileCmd(const string& arg)//增加CopyFile命令处理
{
    //arg格式："c:\1.txt, /os/2.txt"
    size_t commaPos = arg.find(',');//找到逗号位置
    if(commaPos==string::npos)//非法
	{
        std::cout<<"Invalid CopyFile command format. Usage: CopyFile real_path, virtual_path"<<std::endl;
        return;
    }
    string realPath=arg.substr(0,commaPos);//真实地址
    string virtualPath=arg.substr(commaPos+1);//虚拟地址
    
    //去除两边空格
    auto trim=[](string& s)
	{
        size_t start=s.find_first_not_of(" \t");
        size_t end=s.find_last_not_of(" \t");
        if(start==string::npos)
			s="";
        else s=s.substr(start,end-start+1);
    };
    trim(realPath);
    trim(virtualPath);

    string content;
    if(!ReadRealFile(realPath, content))//获得真实文件内容
	{
        cout<<"Failed to read real file: "<<realPath<<endl;
        return;
    }

    int fileBlock=findEntryBlock(virtualPath);//找目标位置
    if (fileBlock<0)//文件不存在，创建文件
	{
        CreateFile(virtualPath);
        fileBlock=findEntryBlock(virtualPath);//更新fileblock
        if(fileBlock<0)
		{
            cout<<"Failed to create virtual file: "<<virtualPath<<endl;
            return;
        }
    } 
	else//文件存在
	{
        DirEntryDisk e=readDirEntry(fileBlock);//获得文件目录
        if(e.isDir)//如果是文件夹
		{
            cout<<"Virtual path is a directory, not a file: "<<virtualPath<<endl;
            return;
        }
    }

    //写入内容
    WriteFile(virtualPath, content);
    cout<<"CopyFile success: "<<realPath<<" -> "<<virtualPath<<endl;
}


void ReadFile(const string& path)//读文件内容（输出到控制台）
{
    int fileBlock=findEntryBlock(path);//获得文件地址
    if(fileBlock<0)
	{
        cout<< "File not found." <<endl;
        return;
    }
    DirEntryDisk fileEntry=readDirEntry(fileBlock);//获得文件目录
    if(fileEntry.isDir)//如果是文件夹
	{
        cout<<"Path is a directory, not a file."<<endl;
        return;
    }

    int cur=fileEntry.firstBlock;//定位到文件内容的开头块
    fstream fs(DISK_FILE, ios::binary|ios::in);//打开磁盘
    while(cur!=-1)//如果还有没读的块
	{
        fs.seekg(cur*BLOCK_SIZE, ios::beg);//找到位置
        FileDataBlock block;//创建文件内容对象
        fs.read(reinterpret_cast<char*>(&block), sizeof(FileDataBlock));//读
        cout.write(block.data, min(fileEntry.size, (int)sizeof(block.data)));//输出
        fileEntry.size-=(int)sizeof(block.data);//剩余未读的文件大小减小
        cur=block.nextBlock;//进入下一个块
    }
    fs.close();
    cout<<endl;
}

int main() 
{
    cout<<"Commands List:"<<endl<<endl;
	cout<<"Format"<<endl;
	cout<<"Tree <path>"<<endl;
	cout<<"CreateDir <path>"<<endl;
	cout<<"DeleteDir <path>"<<endl;
    cout<<"CreateFile <path>"<<endl;
	cout<<"DeleteFile <path>"<<endl;
	cout<<"WriteFile <path> <content>"<<endl;
	cout<<"ReadFile <path>"<<endl;
	cout<<"CopyFile <OriginalFile>, <TargetFile> e.g:c:\1.txt, /os/2.txt"<<endl;
	cout<<"FreeSpace"<<endl;
	cout<<"Exit"<<endl;
	cout<<endl;

    loadBitmap();
    while(true)
	{
        cout<<"> ";
        string line;
        if(!getline(cin, line)) 
			break;
        string cmd, arg1, arg2="";
        stringstream ss(line);
        ss>>cmd;
        if(cmd=="Format")
		{
            Format();
            loadBitmap();
        } 
		else if(cmd=="Tree") 
		{
            ss>>arg1;
            Tree(arg1);
        } 
		else if(cmd=="CreateDir") 
		{
            ss>>arg1;
            CreateDir(arg1);
        } 
		else if(cmd=="DeleteDir") 
		{
            ss>>arg1;
            DeleteDir(arg1);
        } 
		else if(cmd=="CreateFile") 
		{
            ss>>arg1;
            CreateFile(arg1);
        } 
		else if(cmd=="DeleteFile") 
		{
            ss>>arg1;
            DeleteFile(arg1);
        } 
		else if (cmd=="WriteFile")
		{
            ss>>arg1;
            getline(ss,arg2);
            if(!arg2.empty()&&arg2[0]==' ')
				arg2.erase(0, 1);
            WriteFile(arg1, arg2);
        } 
		else if(cmd=="ReadFile")
		{
            ss>>arg1;
            ReadFile(arg1);
        } 
		else if(cmd=="FreeSpace")
            cout<<"Free space: "<<FreeSpace()<<" KB"<<endl;
		else if (cmd=="CopyFile")
		{
    		string rest;
    		getline(ss, rest);
    		if(!rest.empty()&&rest[0]==' ')
				rest.erase(0, 1);
    		CopyFileCmd(rest);
		} 
		else if(cmd=="Exit")
            break;
        else
            cout<<"Unknown command."<<endl;
    }
    return 0;
}
