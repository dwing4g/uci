////////////////////////////////////////////////////////////////////////
//  buffer.h [by dwing] (2009.2.15)
////////////////////////////////////////////////////////////////////////
#pragma once
#include <stdlib.h>
#include <string.h>
////////////////////////////////////////////////////////////////////////
#ifndef TYPE_U8U16U32
#define TYPE_U8U16U32
typedef unsigned char   U8;
typedef unsigned short  U16;
typedef unsigned int    U32;
#endif
////////////////////////////////////////////////////////////////////////
class Buffer //缓冲区类(字节队列,单线程版)
{
public:
	enum {BLOCKSIZE = 65500}; //缓冲区块大小
private:
	struct Block
	{
		U8 data[BLOCKSIZE];
		Block *next;
	};
	Block *head, *tail;
	U32 headpos, tailpos, bufsize;
public:
	Buffer()
	{
		tail = head = (Block*)malloc(sizeof(Block));
		bufsize = tailpos = headpos = 0;
		head->next = 0;
	}
	~Buffer()
	{
		reset();
		free(head);
	}
	void reset()
	{
		while(head->next)
		{
			Block *temp = head->next;
			free(head);
			head = temp;
		}
		bufsize = tailpos = headpos = 0;
	}
	U32 size() const //取当前缓冲区的大小
	{
		return bufsize;
	}
	bool write(U8 *data, U32 size) //写缓冲区
	{
		while(size)
		{
			if(tailpos == BLOCKSIZE)
			{
				Block *temp = (Block*)malloc(sizeof(Block));
				if(!temp) return false;
				tail = tail->next = temp;
				tail->next = 0;
				tailpos = 0;
			}
			U32 len = (size > BLOCKSIZE-tailpos) ? (BLOCKSIZE-tailpos) : size;
			memcpy(tail->data + tailpos, data, len);
			data += len; tailpos += len;
			size -= len; bufsize += len;
		}
		return true;
	}
	bool read(U8 *data, U32 size) //读缓冲区
	{
		if(size > bufsize) return false;
		while(size)
		{
			if(headpos == BLOCKSIZE)
			{
				Block *temp = head->next;
				free(head);
				head = temp;
				headpos = 0;
			}
			U32 len = (size > BLOCKSIZE-headpos) ? (BLOCKSIZE-headpos) : size;
			memcpy(data, head->data + headpos, len);
			data += len; headpos += len;
			size -= len; bufsize -= len;
		}
		return true;
	}
	void skip(U32 size)
	{
		if(size > bufsize)
			reset();
		else
		{
			while(size)
			{
				if(headpos == BLOCKSIZE)
				{
					Block *temp = head->next;
					free(head);
					head = temp;
					headpos = 0;
				}
				U32 len = (size > BLOCKSIZE-headpos) ? (BLOCKSIZE-headpos) : size;
				headpos += len;
				size -= len; bufsize -= len;
			}
		}
	}
	Buffer& operator<<(Buffer& buf)
	{
		const U32 BUFFERSIZE = 256;
		U8 b[BUFFERSIZE];
		while(buf.size())
		{
			U32 len = (buf.size() > BUFFERSIZE) ? BUFFERSIZE : buf.size();
			buf.read(b,len);
			   write(b,len);
		}
		return *this;
	}
	Buffer& operator<<(const U8& c) //写缓冲区(1字节)
	{
		if(tailpos == BLOCKSIZE)
		{
			Block *temp = (Block*)malloc(sizeof(Block));
			tail = tail->next = temp;
			tail->next = 0;
			tailpos = 0;
		}
		tail->data[tailpos++] = c;
		++bufsize;
		return *this;
	}
	Buffer& operator<<(const char *s) //写缓冲区(1字节)
	{
		write((U8*)s,strlen(s));
		return *this;
	}
	Buffer& operator>>(U8& c) //读缓冲区(1字节)
	{
		if(bufsize)
		{
			if(headpos == BLOCKSIZE)
			{
				Block *temp = head->next;
				free(head);
				head = temp;
				headpos = 0;
			}
			c = head->data[headpos++];
			--bufsize;
		}
		else c = 0;
		return *this;
	}
	U8 pop() //从队头读缓冲区(1字节)
	{
		U8 c=0;
		if(bufsize)
		{
			c = tail->data[--tailpos];
			if(!tailpos && head!=tail)
			{
				Block *temp = head;
				while(temp->next!=tail)
					temp = temp->next;
				free(tail);
				tail = temp;
				tail->next = 0;
				tailpos = BLOCKSIZE;
			}
			--bufsize;
		}
		return c;
	}
	U8 operator[](U32 pos)
	{
		if(pos > bufsize) return 0;
		U32 i = BLOCKSIZE - headpos;
		Block* temp = head;
		while(pos >= i)
		{
			pos -= i;
			i = BLOCKSIZE;
			temp = temp->next;
		}
		return temp->data[BLOCKSIZE - (i - pos)];
	}
	void set(U32 pos, U8 v)
	{
		if(pos > bufsize) return;
		U32 i = BLOCKSIZE - headpos;
		Block* temp = head;
		while(pos >= i)
		{
			pos -= i;
			i = BLOCKSIZE;
			temp = temp->next;
		}
		temp->data[BLOCKSIZE - (i - pos)] = v;
	}
};
////////////////////////////////////////////////////////////////////////
