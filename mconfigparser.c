#include "mconfigfile.h"

#define YYSTACKDEPTH 100
#define YYNSTATE 68
#define YYNRULE 43
#define YY_NO_ACTION	(YYNSTATE + YYNRULE + 2)
#define YY_ERROR_ACTION (YYNSTATE + YYNRULE)
#define YYNOCODE 50

#define YYACTIONTYPE unsigned char
#define YYCODETYPE	unsigned char

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


static YYACTIONTYPE yy_action[] = {
	/*     0 */     2, 3, 4, 5, 14, 15, 112, 1, 7, 46,
	/*    10 */    68, 16, 103, 17, 21, 47, 16, 42, 11, 21,
	/*    20 */    18, 39, 41, 9, 10, 12, 47, 41, 93, 63,
	/*    30 */    13, 47, 11, 64, 59, 61, 29, 28, 38, 59,
	/*    40 */    61, 31, 26, 23, 35, 16, 20, 8, 21, 36,
	/*    50 */    16, 20, 108, 21, 32, 33, 41, 20, 45, 102,
	/*    60 */    47, 41, 44, 67, 48, 47, 95, 52, 59, 61,
	/*    70 */    29, 28, 34, 59, 61, 43, 26, 23, 35, 21,
	/*    80 */    53, 24, 25, 27, 30, 49, 29, 50, 6, 29,
	/*    90 */    50, 65, 26, 23, 51, 26, 23, 60, 66, 29,
	/*   100 */    50, 94, 94, 94, 94, 26, 23, 62, 94, 94,
	/*   110 */    21, 94, 24, 25, 27, 29, 19, 29, 37, 29,
	/*   120 */    40, 26, 23, 26, 23, 26, 23, 55, 56, 57,
	/*   130 */    58, 29, 54, 22, 94, 94, 94, 26, 23, 26,
	/*   140 */    23,
};

static YYCODETYPE yy_lookahead[] = {
	/*     0 */    29, 30, 31, 32, 33, 34, 27, 28, 45, 38,
	/*    10 */     0, 1, 13, 42, 4, 16, 1, 46, 47, 4,
	/*    20 */     2, 3, 12, 38, 39, 13, 16, 12, 15, 14,
	/*    30 */    28, 16, 47, 48, 24, 25, 35, 36, 37, 24,
	/*    40 */    25, 40, 41, 42, 43, 1, 5, 15, 4, 11,
	/*    50 */     1, 5, 11, 4, 9, 10, 12, 5, 14, 13,
	/*    60 */    16, 12, 28, 14, 17, 16, 13, 19, 24, 25,
	/*    70 */    35, 36, 37, 24, 25, 13, 41, 42, 43, 4,
	/*    80 */    44, 6, 7, 8, 9, 18, 35, 36, 1, 35,
	/*    90 */    36, 13, 41, 42, 43, 41, 42, 43, 28, 35,
	/*   100 */    36, 49, 15, 49, 49, 41, 42, 43, 49, 49,
	/*   110 */     4, 49, 6, 7, 8, 35, 36, 35, 36, 35,
	/*   120 */    36, 41, 42, 41, 42, 41, 42, 20, 21, 22,
	/*   130 */    23, 35, 36, 35, 49, 49, 49, 41, 42, 41,
	/*   140 */    42,
};

#define YY_SHIFT_USE_DFLT (-2)
static signed char yy_shift_ofst[] = {
	/*     0 */    -2, 10, -2, -2, -2, 87, 13, 32, -1, -2,
	/*    10 */    -2, 12, -2, 15, -2, -2, -2, 18, 106, 52,
	/*    20 */   106, -2, -2, -2, -2, -2, -2, 75, 41, -2,
	/*    30 */    -2, 45, -2, 106, -2, 38, 106, 52, -2, 106,
	/*    40 */    52, 53, 62, -2, 44, -2, -2, 47, 67, 106,
	/*    50 */    52, 48, 107, 106, 46, -2, -2, -2, -2, 106,
	/*    60 */    -2, 106, -2, -2, 78, -2, 49, -2,
};


static YYACTIONTYPE yy_default[] = {
	/*     0 */    70, 111, 69, 71, 72, 111, 73, 111, 111, 97,
	/*    10 */    98, 111, 70, 111, 74, 75, 76, 111, 111, 77,
	/*    20 */   111, 79, 80, 82, 83, 84, 85, 111, 91, 81,
	/*    30 */    86, 111, 87, 89, 88, 111, 111, 92, 90, 111,
	/*    40 */    78, 111, 111, 70, 111, 96, 99, 111, 111, 111,
	/*    50 */   108, 111, 111, 111, 111, 104, 105, 106, 107, 111,
	/*    60 */   109, 111, 110, 100, 111, 70, 111, 101,
};

#define YY_SZ_ACTTAB (sizeof(yy_action)/sizeof(yy_action[0]))


void* configparserAlloc(void* (*mallocproc)(size_t)){
	yyParser* pParser;
	pParser = (yyParser*)mallocproc(sizeof(*pParser));
	force_assert(pParser != NULL);
	pParser->yyidx = -1;
	return pParser;
}


static int yy_find_shift_action(yyParser* pParser, int iLookAhead){
	int i;
	int stateno = pParser->yystack[pParser->yyidx].stateno;

	i = yy_shift_ofst[stateno];
	if (i == YY_SHIFT_USE_DFLT){
		return yy_default[stateno];
	}

	if (iLookAhead == YYNOCODE){
		return YY_NO_ACTION;
	}

	i += iLookAhead;
	if (i < 0 || i >= YY_SZ_ACTTAB || yy_lookahead[i] != iLookAhead){
#ifdef YYFALLBACK
		int iFallBack;
		if (iLookAhead < sizeof(yyFallback) / sizeof(yyFallback[0]) &&
			(iFallBack=yyFallback[iLookAhead]) != 0){

			return yy_find_shift_action(pParser, iFallBack);
		}
#endif
		return yy_default[stateno];
	}else {
		return yy_action[i];
	}
}


void configparser(void* yyp, int yymajor, buffer* yyminor, config_t* ctx){
	YYMINORTYPE yyminorunion;
	int yyendofinput;
	int yyact;

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
	pParser->context = ctx;

	do{
		yyact = yy_find_shift_action(pParser, yymajor);

		if (yyact < YYNSTATE){
		
		}else if (yyact < YYNSTATE + YYNRULE){
		
		}else if (yyact == YY_ERROR_ACTION){
		
		}else{
		
		}
	
	} while (yymajor != YYNOCODE && pParser->yyidx > 0);
	return;
}