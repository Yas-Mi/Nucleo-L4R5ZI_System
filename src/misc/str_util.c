/*
	http://www.ncad.co.jp/~komata/c-kouza23.htm
*/

#include	<stdio.h>
#include	<string.h>
#include "kozos.h"

// strÇÕïKÇ∏ASCIIÇ≈Ç†ÇÈÇ±Ç∆
// à¯êîÇ∆ÇµÇƒìnÇµÇΩstrÇ…åãâ Çäiî[Ç∑ÇÈÇΩÇﬂÅAì¸óÕÇÃÉTÉCÉY*2î{+2ÇÃîzóÒÇìnÇ∑Ç±Ç∆
// ó·) 4ï∂éöÇÃASCIIÇëSäpÇ…ÇµÇΩÇ¢Ç∆Ç´
// ì¸óÕ str[4*2+2] = '1','2','3','4','\0'
// èoóÕ str[4*2+2] = (0x82,0x4F),(0x82,0x50),(0x82,0x51),(0x82,0x52),(0x00,0x00)
// (0x00,0x00)ÇÕèIí[ï∂éö
int han2zen(char *str)
{
	char	*buf,*p,*ptr;
	uint8_t size;
	
	// ÉTÉCÉYéÊìæ (*)Åu+2ÅvÇÕèIí[ï∂éö0x0000óp
	size = strlen(str)*2+2;
	
	buf=(char *)kz_kmalloc(size);
	memset(buf, '\0', size);
	
	for(ptr=str,p=buf;*ptr!='\0';*ptr++){
		switch((int)*ptr){
			case    ' ': strcpy(p,"Å@");p+=2;break;
			case    '!': strcpy(p,"ÅI");p+=2;break;
			case    '"': strcpy(p,"Åh");p+=2;break;
			case    '#': strcpy(p,"Åî");p+=2;break;
			case    '$': strcpy(p,"Åê");p+=2;break;
			case    '%': strcpy(p,"Åì");p+=2;break;
			case    '&': strcpy(p,"Åï");p+=2;break;
			case    '\'': strcpy(p,"Åf");p+=2;break;
			case    '(': strcpy(p,"Åi");p+=2;break;
			case    ')': strcpy(p,"Åj");p+=2;break;
			case    '*': strcpy(p,"Åñ");p+=2;break;
			case    '+': strcpy(p,"Å{");p+=2;break;
			case    ',': strcpy(p,"ÅC");p+=2;break;
			case    '-': strcpy(p,"Å|");p+=2;break;
			case    '.': strcpy(p,"ÅD");p+=2;break;
			case    '/': strcpy(p,"Å^");p+=2;break;
			case    '0': strcpy(p,"ÇO");p+=2;break;
			case    '1': strcpy(p,"ÇP");p+=2;break;
			case    '2': strcpy(p,"ÇQ");p+=2;break;
			case    '3': strcpy(p,"ÇR");p+=2;break;
			case    '4': strcpy(p,"ÇS");p+=2;break;
			case    '5': strcpy(p,"ÇT");p+=2;break;
			case    '6': strcpy(p,"ÇU");p+=2;break;
			case    '7': strcpy(p,"ÇV");p+=2;break;
			case    '8': strcpy(p,"ÇW");p+=2;break;
			case    '9': strcpy(p,"ÇX");p+=2;break;
			case    ':': strcpy(p,"ÅF");p+=2;break;
			case    ';': strcpy(p,"ÅG");p+=2;break;
			case    '<': strcpy(p,"ÅÉ");p+=2;break;
			case    '=': strcpy(p,"ÅÅ");p+=2;break;
			case    '>': strcpy(p,"ÅÑ");p+=2;break;
			case    '?': strcpy(p,"ÅH");p+=2;break;
			case    '@': strcpy(p,"Åó");p+=2;break;
			case    'A': strcpy(p,"Ç`");p+=2;break;
			case    'B': strcpy(p,"Ça");p+=2;break;
			case    'C': strcpy(p,"Çb");p+=2;break;
			case    'D': strcpy(p,"Çc");p+=2;break;
			case    'E': strcpy(p,"Çd");p+=2;break;
			case    'F': strcpy(p,"Çe");p+=2;break;
			case    'G': strcpy(p,"Çf");p+=2;break;
			case    'H': strcpy(p,"Çg");p+=2;break;
			case    'I': strcpy(p,"Çh");p+=2;break;
			case    'J': strcpy(p,"Çi");p+=2;break;
			case    'K': strcpy(p,"Çj");p+=2;break;
			case    'L': strcpy(p,"Çk");p+=2;break;
			case    'M': strcpy(p,"Çl");p+=2;break;
			case    'N': strcpy(p,"Çm");p+=2;break;
			case    'O': strcpy(p,"Çn");p+=2;break;
			case    'P': strcpy(p,"Ço");p+=2;break;
			case    'Q': strcpy(p,"Çp");p+=2;break;
			case    'R': strcpy(p,"Çq");p+=2;break;
			case    'S': strcpy(p,"Çr");p+=2;break;
			case    'T': strcpy(p,"Çs");p+=2;break;
			case    'U': strcpy(p,"Çt");p+=2;break;
			case    'V': strcpy(p,"Çu");p+=2;break;
			case    'W': strcpy(p,"Çv");p+=2;break;
			case    'X': strcpy(p,"Çw");p+=2;break;
			case    'Y': strcpy(p,"Çx");p+=2;break;
			case    'Z': strcpy(p,"Çy");p+=2;break;
			case    '[': strcpy(p,"Åm");p+=2;break;
			case    '\\': strcpy(p,"Åè");p+=2;break;
			case    ']': strcpy(p,"Ån");p+=2;break;
			case    '^': strcpy(p,"ÅO");p+=2;break;
			case    '_': strcpy(p,"ÅQ");p+=2;break;
			case    '`': strcpy(p,"Åe");p+=2;break;
			case    'a': strcpy(p,"ÇÅ");p+=2;break;
			case    'b': strcpy(p,"ÇÇ");p+=2;break;
			case    'c': strcpy(p,"ÇÉ");p+=2;break;
			case    'd': strcpy(p,"ÇÑ");p+=2;break;
			case    'e': strcpy(p,"ÇÖ");p+=2;break;
			case    'f': strcpy(p,"ÇÜ");p+=2;break;
			case    'g': strcpy(p,"Çá");p+=2;break;
			case    'h': strcpy(p,"Çà");p+=2;break;
			case    'i': strcpy(p,"Çâ");p+=2;break;
			case    'j': strcpy(p,"Çä");p+=2;break;
			case    'k': strcpy(p,"Çã");p+=2;break;
			case    'l': strcpy(p,"Çå");p+=2;break;
			case    'm': strcpy(p,"Çç");p+=2;break;
			case    'n': strcpy(p,"Çé");p+=2;break;
			case    'o': strcpy(p,"Çè");p+=2;break;
			case    'p': strcpy(p,"Çê");p+=2;break;
			case    'q': strcpy(p,"Çë");p+=2;break;
			case    'r': strcpy(p,"Çí");p+=2;break;
			case    's': strcpy(p,"Çì");p+=2;break;
			case    't': strcpy(p,"Çî");p+=2;break;
			case    'u': strcpy(p,"Çï");p+=2;break;
			case    'v': strcpy(p,"Çñ");p+=2;break;
			case    'w': strcpy(p,"Çó");p+=2;break;
			case    'x': strcpy(p,"Çò");p+=2;break;
			case    'y': strcpy(p,"Çô");p+=2;break;
			case    'z': strcpy(p,"Çö");p+=2;break;
			case    '{': strcpy(p,"Åo");p+=2;break;
			case    '|': strcpy(p,"Åb");p+=2;break;
			case    '}': strcpy(p,"Åp");p+=2;break;
			default:
				*p=*ptr;
				p++;
				*p='\0';
				break;
			
		}
	}
	//strcpy(str,buf);
	memcpy(str, buf, size);
	kz_kmfree(buf);
	
	return(0);
}

int zen2han(char *str)
{
	char	*buf,*p,*ptr;
	
	buf=(char *)calloc(strlen(str)+1,sizeof(char));
	
	for(ptr=str,p=buf;*ptr!='\0';*ptr++){
		if(strncmp(ptr,"Å@",2)==0){*p=' ';p++;ptr++;}
		else if(strncmp(ptr,"ÅI",2)==0){*p='!';p++;ptr++;}
		else if(strncmp(ptr,"Åh",2)==0){*p='"';p++;ptr++;}
		else if(strncmp(ptr,"Åî",2)==0){*p='#';p++;ptr++;}
		else if(strncmp(ptr,"Åê",2)==0){*p='$';p++;ptr++;}
		else if(strncmp(ptr,"Åì",2)==0){*p='%';p++;ptr++;}
		else if(strncmp(ptr,"Åï",2)==0){*p='&';p++;ptr++;}
		else if(strncmp(ptr,"Åf",2)==0){*p='\'';p++;ptr++;}
		else if(strncmp(ptr,"Åi",2)==0){*p='(';p++;ptr++;}
		else if(strncmp(ptr,"Åj",2)==0){*p=')';p++;ptr++;}
		else if(strncmp(ptr,"Åñ",2)==0){*p='*';p++;ptr++;}
		else if(strncmp(ptr,"Å{",2)==0){*p='+';p++;ptr++;}
		else if(strncmp(ptr,"ÅC",2)==0){*p=',';p++;ptr++;}
		else if(strncmp(ptr,"Å|",2)==0){*p='-';p++;ptr++;}
		else if(strncmp(ptr,"ÅD",2)==0){*p='.';p++;ptr++;}
		else if(strncmp(ptr,"Å^",2)==0){*p='/';p++;ptr++;}
		else if(strncmp(ptr,"ÇO",2)==0){*p='0';p++;ptr++;}
		else if(strncmp(ptr,"ÇP",2)==0){*p='1';p++;ptr++;}
		else if(strncmp(ptr,"ÇQ",2)==0){*p='2';p++;ptr++;}
		else if(strncmp(ptr,"ÇR",2)==0){*p='3';p++;ptr++;}
		else if(strncmp(ptr,"ÇS",2)==0){*p='4';p++;ptr++;}
		else if(strncmp(ptr,"ÇT",2)==0){*p='5';p++;ptr++;}
		else if(strncmp(ptr,"ÇU",2)==0){*p='6';p++;ptr++;}
		else if(strncmp(ptr,"ÇV",2)==0){*p='7';p++;ptr++;}
		else if(strncmp(ptr,"ÇW",2)==0){*p='8';p++;ptr++;}
		else if(strncmp(ptr,"ÇX",2)==0){*p='9';p++;ptr++;}
		else if(strncmp(ptr,"ÅF",2)==0){*p=':';p++;ptr++;}
		else if(strncmp(ptr,"ÅG",2)==0){*p=';';p++;ptr++;}
		else if(strncmp(ptr,"ÅÉ",2)==0){*p='<';p++;ptr++;}
		else if(strncmp(ptr,"ÅÅ",2)==0){*p='=';p++;ptr++;}
		else if(strncmp(ptr,"ÅÑ",2)==0){*p='>';p++;ptr++;}
		else if(strncmp(ptr,"ÅH",2)==0){*p='?';p++;ptr++;}
		else if(strncmp(ptr,"Åó",2)==0){*p='@';p++;ptr++;}
		else if(strncmp(ptr,"Ç`",2)==0){*p='A';p++;ptr++;}
		else if(strncmp(ptr,"Ça",2)==0){*p='B';p++;ptr++;}
		else if(strncmp(ptr,"Çb",2)==0){*p='C';p++;ptr++;}
		else if(strncmp(ptr,"Çc",2)==0){*p='D';p++;ptr++;}
		else if(strncmp(ptr,"Çd",2)==0){*p='E';p++;ptr++;}
		else if(strncmp(ptr,"Çe",2)==0){*p='F';p++;ptr++;}
		else if(strncmp(ptr,"Çf",2)==0){*p='G';p++;ptr++;}
		else if(strncmp(ptr,"Çg",2)==0){*p='H';p++;ptr++;}
		else if(strncmp(ptr,"Çh",2)==0){*p='I';p++;ptr++;}
		else if(strncmp(ptr,"Çi",2)==0){*p='J';p++;ptr++;}
		else if(strncmp(ptr,"Çj",2)==0){*p='K';p++;ptr++;}
		else if(strncmp(ptr,"Çk",2)==0){*p='L';p++;ptr++;}
		else if(strncmp(ptr,"Çl",2)==0){*p='M';p++;ptr++;}
		else if(strncmp(ptr,"Çm",2)==0){*p='N';p++;ptr++;}
		else if(strncmp(ptr,"Çn",2)==0){*p='O';p++;ptr++;}
		else if(strncmp(ptr,"Ço",2)==0){*p='P';p++;ptr++;}
		else if(strncmp(ptr,"Çp",2)==0){*p='Q';p++;ptr++;}
		else if(strncmp(ptr,"Çq",2)==0){*p='R';p++;ptr++;}
		else if(strncmp(ptr,"Çr",2)==0){*p='S';p++;ptr++;}
		else if(strncmp(ptr,"Çs",2)==0){*p='T';p++;ptr++;}
		else if(strncmp(ptr,"Çt",2)==0){*p='U';p++;ptr++;}
		else if(strncmp(ptr,"Çu",2)==0){*p='V';p++;ptr++;}
		else if(strncmp(ptr,"Çv",2)==0){*p='W';p++;ptr++;}
		else if(strncmp(ptr,"Çw",2)==0){*p='X';p++;ptr++;}
		else if(strncmp(ptr,"Çx",2)==0){*p='Y';p++;ptr++;}
		else if(strncmp(ptr,"Çy",2)==0){*p='Z';p++;ptr++;}
		else if(strncmp(ptr,"Åm",2)==0){*p='[';p++;ptr++;}
		else if(strncmp(ptr,"Åè",2)==0){*p='\\';p++;ptr++;}
		else if(strncmp(ptr,"Ån",2)==0){*p=']';p++;ptr++;}
		else if(strncmp(ptr,"ÅO",2)==0){*p='^';p++;ptr++;}
		else if(strncmp(ptr,"ÅQ",2)==0){*p='_';p++;ptr++;}
		else if(strncmp(ptr,"Åe",2)==0){*p='`';p++;ptr++;}
		else if(strncmp(ptr,"ÇÅ",2)==0){*p='a';p++;ptr++;}
		else if(strncmp(ptr,"ÇÇ",2)==0){*p='b';p++;ptr++;}
		else if(strncmp(ptr,"ÇÉ",2)==0){*p='c';p++;ptr++;}
		else if(strncmp(ptr,"ÇÑ",2)==0){*p='d';p++;ptr++;}
		else if(strncmp(ptr,"ÇÖ",2)==0){*p='e';p++;ptr++;}
		else if(strncmp(ptr,"ÇÜ",2)==0){*p='f';p++;ptr++;}
		else if(strncmp(ptr,"Çá",2)==0){*p='g';p++;ptr++;}
		else if(strncmp(ptr,"Çà",2)==0){*p='h';p++;ptr++;}
		else if(strncmp(ptr,"Çâ",2)==0){*p='i';p++;ptr++;}
		else if(strncmp(ptr,"Çä",2)==0){*p='j';p++;ptr++;}
		else if(strncmp(ptr,"Çã",2)==0){*p='k';p++;ptr++;}
		else if(strncmp(ptr,"Çå",2)==0){*p='l';p++;ptr++;}
		else if(strncmp(ptr,"Çç",2)==0){*p='m';p++;ptr++;}
		else if(strncmp(ptr,"Çé",2)==0){*p='n';p++;ptr++;}
		else if(strncmp(ptr,"Çè",2)==0){*p='o';p++;ptr++;}
		else if(strncmp(ptr,"Çê",2)==0){*p='p';p++;ptr++;}
		else if(strncmp(ptr,"Çë",2)==0){*p='q';p++;ptr++;}
		else if(strncmp(ptr,"Çí",2)==0){*p='r';p++;ptr++;}
		else if(strncmp(ptr,"Çì",2)==0){*p='s';p++;ptr++;}
		else if(strncmp(ptr,"Çî",2)==0){*p='t';p++;ptr++;}
		else if(strncmp(ptr,"Çï",2)==0){*p='u';p++;ptr++;}
		else if(strncmp(ptr,"Çñ",2)==0){*p='v';p++;ptr++;}
		else if(strncmp(ptr,"Çó",2)==0){*p='w';p++;ptr++;}
		else if(strncmp(ptr,"Çò",2)==0){*p='x';p++;ptr++;}
		else if(strncmp(ptr,"Çô",2)==0){*p='y';p++;ptr++;}
		else if(strncmp(ptr,"Çö",2)==0){*p='z';p++;ptr++;}
		else if(strncmp(ptr,"Åo",2)==0){*p='{';p++;ptr++;}
		else if(strncmp(ptr,"Åb",2)==0){*p='|';p++;ptr++;}
		else if(strncmp(ptr,"Åp",2)==0){*p='}';p++;ptr++;}
		else{ *p=*ptr; p++; }
	}
	strcpy(str,buf);
	free(buf);
	
	return(0);
}

// ì¡íËÇÃï∂éöà íuÇéÊìæ
uint8_t find_str(char str, uint8_t *data)
{
	uint8_t *start_addr, *end_addr;
	uint8_t pos = 0;
	
	// äJénÉAÉhÉåÉXÇéÊìæ
	start_addr = data;
	
	// ì¡íËÇÃï∂éöÇÃà íuÇéÊìæ
	end_addr = strchr(data, str);
	
	if (end_addr != NULL) {
		pos = (uint8_t)(end_addr - start_addr);
	}
	
	return pos;
}

// ÉAÉXÉLÅ[Ç©ÇÁêÆêîÇ…ïœä∑
uint8_t ascii2num(char str)
{
	uint8_t ret_val = 0xFF;
	
	if (('0' <= str) && (str <= '9')) {
		ret_val = str - 0x30;
	}
	
	return ret_val;
}

// êÆêîÇ©ÇÁÉAÉXÉLÅ[Ç…ïœä∑
uint8_t num2ascii(uint8_t val)
{
	uint8_t ret_char = 0xFF;
	
	if ((0 <= val) && (val <= 9)) {
		ret_char = val + 0x30;
	}
	
	return ret_char;
}
