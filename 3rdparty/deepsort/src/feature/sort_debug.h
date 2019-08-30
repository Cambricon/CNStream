#ifndef __SORT_DEBUG_H__
#define __SORT_DEBUG_H__

//#define SORT_DEBUG_EN

#ifdef SORT_DEBUG_EN
#define TAG_CODE printf("%s.%s.%d\n", __FILE__, __FUNCTION__, __LINE__)
#define PRINT_MATRIX(matrix) {\
    printf("%s, %dx%d\n", #matrix, (int)matrix.rows(), (int)matrix.columns());\
    printf("-----------------------------------------\n");\
    for(int iloop=0;iloop<(int)matrix.rows();iloop++)\
    {\
        for(int kloop=0;kloop<(int)matrix.columns();kloop++)\
        {\
            printf("%-8.2f", (float)matrix(iloop, kloop));\
        }\
        printf("\n");\
    }\
    printf("\n");\
}

#define PRINT_DYNAMICM(matrix) {\
    printf("%s, %dx%d\n", #matrix, (int)matrix.rows(), (int)matrix.cols());\
    printf("-----------------------------------------\n");\
    for(int iloop=0;iloop<(int)matrix.rows();iloop++)\
    {\
        for(int kloop=0;kloop<(int)matrix.cols();kloop++)\
        {\
            printf("%-8.2f", (float)matrix(iloop, kloop));\
        }\
        printf("\n");\
    }\
    printf("\n");\
}

#define PRINT_DETECTIONS(x) {\
    int index=0;\
    printf("%s:\n", #x);\
    printf("-----------------------------------------\n");\
    for(const DETECTION_ROW& detection_row : x)\
    {\
        printf("%d,%f,(%d,%d,%d,%d)\n", \
                index, \
				(float)detection_row.confidence,\
				(int)detection_row.tlwh(0),\
				(int)detection_row.tlwh(1),\
				(int)detection_row.tlwh(2),\
				(int)detection_row.tlwh(3));\
        index++;\
    }\
    printf("\n");\
}

#define PRINT_VECTOR_INT(x) {\
    printf("%s[%d]:", #x, (int)x.size());\
    for(int x_loop : x)\
    {\
        printf("%d,", x_loop);\
    }\
    printf("\n");\
}

#define PRINT_MATCH_DATA(x) {\
    printf("%s[%d]:", #x, (int)x.size());\
    for(MATCH_DATA x_loop : x)\
    {\
        printf("[%d,%d], ", x_loop.first, x_loop.second);\
    }\
    printf("\n");\
}
#else
#define TAG_CODE
#define PRINT_MATRIX(matrix)
#define PRINT_DYNAMICM(matrix)
#define PRINT_DETECTIONS(x)
#define PRINT_VECTOR_INT(x)

#endif

#endif