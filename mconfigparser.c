#include "mconfigfile.h"

#define YYSTACKDEPTH 100
#define configparserARG_SDECL config_t* context;

#define configparserTOKENTYPE buffer* 

typedef union{
	configparserTOKENTYPE yy0;
	data_config* yy2;
	array* yy22;
	data_unset* yy59;
	config_cond_t yy75;
	buffer* yy87;
	int yy99;
}YYMINORTYPE;


typedef struct{
	int stateno;
	int major;
	YYMINORTYPE minor;
}yyStackEntry;


typedef struct{
	int yyidx;
	int yyerrcnt;
	configparserARG_SDECL
	yyStackEntry yystack[YYSTACKDEPTH];
}yyParser;


void* configparserAlloc(void* (*mallocproc)(size_t)){
	yyParser* pParser;
	pParser = (yyParser*)mallocproc(sizeof(*pParser));
	force_assert(pParser != NULL);
	pParser->yyidx = -1;
	return pParser;
}



void configparser(void* yyp, int yymajor, buffer* yyminor, config_t* ctx){
	YYMINORTYPE yyminorunion;
	int yyendofinput;

	yyParser* pParser = (yyParser*)yyp;
	if (pParser->yyidx < 0){
		if (yymajor == 0)	return;
		pParser->yyidx = 0;
		pParser->yyerrcnt = -1;
		pParser->yystack[0].stateno = 0;
		pParser->yystack[0].major = 0;
	}

	yyminorunion.yy0 = yyminor;
	yyendofinput = (yymajor == 0);
}