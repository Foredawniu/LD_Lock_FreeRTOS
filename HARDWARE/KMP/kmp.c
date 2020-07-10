#include "kmp.h"

#include "stm32f10x.h"

#include "string.h"



//借鉴来源：https://blog.csdn.net/weixin_43904021/article/details/87792889


void getnext(char *p,int *next)
{
	int len=strlen(p);
	int k=-1;
	int j=0;
	next[0]=-1;
    
    
	//printf("\n j= %d next[0]%d",j,next[j]);
	while(j<len-1)
	{
		if(k==-1||p[k]==p[j])
		{
			k++;
			j++;
			next[j]=k;
			//printf("\n j= %d next[%d]%d",j,j,next[j]);
		}
		else
        {
			k=next[k];
		}
	}
}

int kmp(char *s,char *p,int *next)
{
	int i = 0;
	int j = 0;
	int sLen = strlen(s);
	int pLen = strlen(p);
    
    
	while(i<sLen&&j<pLen) 
	{
		if (j == -1 || s[i] == p[j])
		{
			i++;
			j++;
		}
		else
		{
			//②如果j != -1，且当前字符匹配失败（即S[i] != P[j]），则令 i 不变，j = next[j]    
			//next[j]即为j所对应的next值      
			j=next[j];
        }
	}
	if(j>=pLen)
		return(i-j);
	return 0xffff;
}

u8 MatchCmd_FromESP32(char* strings, char *cmd)
{
    int next[50];
    int position = 0;
    
    getnext(cmd,next);
    position=kmp(strings,cmd,next);
    
    if(position!=0xffff)
    {
        if(position>=4)
        {
            if(strings[position-4]=='L')
                if(strings[position-3]=='C')
                    if(strings[position-2]=='C')
                        if(strings[position-1]=='_')
                            return 1;
        }
    }
    return 0;
}

u8 MatchString_FromESP32(char* strings, char *cmd)
{
    int next[50];
    int position = 0;
    
    getnext(cmd,next);
    position=kmp(strings,cmd,next);
    
    if(position!=0xffff)
    {
        if(position)
        {
            return 1;
        }
    }
    return 0;
}

u8 MatchString(char* strings, char *cmd)
{
    int next[50];
    int position = 0;
    
    getnext(cmd,next);
    position=kmp(strings,cmd,next);
    
    if(position!=0xffff)
    {
        if(position)
        {
            return 1;
        }
    }
    return 0;
}
