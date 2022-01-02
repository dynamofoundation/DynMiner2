#ifndef uint32_t
#define uint32_t unsigned int
#endif


#define F1(x,y,z)   (bitselect(z,y,x))
#define F0(x,y,z)   (bitselect (x, y, ((x) ^ (z))))
#define mod(x,y) ((x)-((x)/(y)*(y)))
#define shr32(x,n) ((x) >> (n))
#define rotl32(a,n) rotate ((a), (n))

#define S0(x) (rotl32 ((x), 25u) ^ rotl32 ((x), 14u) ^ shr32 ((x),  3u))
#define S1(x) (rotl32 ((x), 15u) ^ rotl32 ((x), 13u) ^ shr32 ((x), 10u))
#define S2(x) (rotl32 ((x), 30u) ^ rotl32 ((x), 19u) ^ rotl32 ((x), 10u))
#define S3(x) (rotl32 ((x), 26u) ^ rotl32 ((x), 21u) ^ rotl32 ((x),  7u))

#define SHA256C00 0x428a2f98u
#define SHA256C01 0x71374491u
#define SHA256C02 0xb5c0fbcfu
#define SHA256C03 0xe9b5dba5u
#define SHA256C04 0x3956c25bu
#define SHA256C05 0x59f111f1u
#define SHA256C06 0x923f82a4u
#define SHA256C07 0xab1c5ed5u
#define SHA256C08 0xd807aa98u
#define SHA256C09 0x12835b01u
#define SHA256C0a 0x243185beu
#define SHA256C0b 0x550c7dc3u
#define SHA256C0c 0x72be5d74u
#define SHA256C0d 0x80deb1feu
#define SHA256C0e 0x9bdc06a7u
#define SHA256C0f 0xc19bf174u
#define SHA256C10 0xe49b69c1u
#define SHA256C11 0xefbe4786u
#define SHA256C12 0x0fc19dc6u
#define SHA256C13 0x240ca1ccu
#define SHA256C14 0x2de92c6fu
#define SHA256C15 0x4a7484aau
#define SHA256C16 0x5cb0a9dcu
#define SHA256C17 0x76f988dau
#define SHA256C18 0x983e5152u
#define SHA256C19 0xa831c66du
#define SHA256C1a 0xb00327c8u
#define SHA256C1b 0xbf597fc7u
#define SHA256C1c 0xc6e00bf3u
#define SHA256C1d 0xd5a79147u
#define SHA256C1e 0x06ca6351u
#define SHA256C1f 0x14292967u
#define SHA256C20 0x27b70a85u
#define SHA256C21 0x2e1b2138u
#define SHA256C22 0x4d2c6dfcu
#define SHA256C23 0x53380d13u
#define SHA256C24 0x650a7354u
#define SHA256C25 0x766a0abbu
#define SHA256C26 0x81c2c92eu
#define SHA256C27 0x92722c85u
#define SHA256C28 0xa2bfe8a1u
#define SHA256C29 0xa81a664bu
#define SHA256C2a 0xc24b8b70u
#define SHA256C2b 0xc76c51a3u
#define SHA256C2c 0xd192e819u
#define SHA256C2d 0xd6990624u
#define SHA256C2e 0xf40e3585u
#define SHA256C2f 0x106aa070u
#define SHA256C30 0x19a4c116u
#define SHA256C31 0x1e376c08u
#define SHA256C32 0x2748774cu
#define SHA256C33 0x34b0bcb5u
#define SHA256C34 0x391c0cb3u
#define SHA256C35 0x4ed8aa4au
#define SHA256C36 0x5b9cca4fu
#define SHA256C37 0x682e6ff3u
#define SHA256C38 0x748f82eeu
#define SHA256C39 0x78a5636fu
#define SHA256C3a 0x84c87814u
#define SHA256C3b 0x8cc70208u
#define SHA256C3c 0x90befffau
#define SHA256C3d 0xa4506cebu
#define SHA256C3e 0xbef9a3f7u
#define SHA256C3f 0xc67178f2u 

__constant uint k_sha256[64] =
{
  SHA256C00, SHA256C01, SHA256C02, SHA256C03,
  SHA256C04, SHA256C05, SHA256C06, SHA256C07,
  SHA256C08, SHA256C09, SHA256C0a, SHA256C0b,
  SHA256C0c, SHA256C0d, SHA256C0e, SHA256C0f,
  SHA256C10, SHA256C11, SHA256C12, SHA256C13,
  SHA256C14, SHA256C15, SHA256C16, SHA256C17,
  SHA256C18, SHA256C19, SHA256C1a, SHA256C1b,
  SHA256C1c, SHA256C1d, SHA256C1e, SHA256C1f,
  SHA256C20, SHA256C21, SHA256C22, SHA256C23,
  SHA256C24, SHA256C25, SHA256C26, SHA256C27,
  SHA256C28, SHA256C29, SHA256C2a, SHA256C2b,
  SHA256C2c, SHA256C2d, SHA256C2e, SHA256C2f,
  SHA256C30, SHA256C31, SHA256C32, SHA256C33,
  SHA256C34, SHA256C35, SHA256C36, SHA256C37,
  SHA256C38, SHA256C39, SHA256C3a, SHA256C3b,
  SHA256C3c, SHA256C3d, SHA256C3e, SHA256C3f,
};

#define SHA256_STEP(F0a,F1a,a,b,c,d,e,f,g,h,x,K)        \
{                                                       \
  h += K + x;                                           \
  h += S3 (e) + F1a (e,f,g);                            \
  d += h;                                               \
  h += S2 (a) + F0a (a,b,c);                            \
}                                                       \



#define SHA256_EXPAND(x,y,z,w) (S1 (x) + y + S0 (z) + w) 

static void sha256_process2 (const unsigned int *W, unsigned int *digest)
{
  unsigned int a = digest[0];
  unsigned int b = digest[1];
  unsigned int c = digest[2];
  unsigned int d = digest[3];
  unsigned int e = digest[4];
  unsigned int f = digest[5];
  unsigned int g = digest[6];
  unsigned int h = digest[7];

  unsigned int w0_t = W[0];
  unsigned int w1_t = W[1];
  unsigned int w2_t = W[2];
  unsigned int w3_t = W[3];
  unsigned int w4_t = W[4];
  unsigned int w5_t = W[5];
  unsigned int w6_t = W[6];
  unsigned int w7_t = W[7];
  unsigned int w8_t = W[8];
  unsigned int w9_t = W[9];
  unsigned int wa_t = W[10];
  unsigned int wb_t = W[11];
  unsigned int wc_t = W[12];
  unsigned int wd_t = W[13];
  unsigned int we_t = W[14];
  unsigned int wf_t = W[15];

  #define ROUND_EXPAND(i)                           \
  {                                                 \
    w0_t = SHA256_EXPAND (we_t, w9_t, w1_t, w0_t);  \
    w1_t = SHA256_EXPAND (wf_t, wa_t, w2_t, w1_t);  \
    w2_t = SHA256_EXPAND (w0_t, wb_t, w3_t, w2_t);  \
    w3_t = SHA256_EXPAND (w1_t, wc_t, w4_t, w3_t);  \
    w4_t = SHA256_EXPAND (w2_t, wd_t, w5_t, w4_t);  \
    w5_t = SHA256_EXPAND (w3_t, we_t, w6_t, w5_t);  \
    w6_t = SHA256_EXPAND (w4_t, wf_t, w7_t, w6_t);  \
    w7_t = SHA256_EXPAND (w5_t, w0_t, w8_t, w7_t);  \
    w8_t = SHA256_EXPAND (w6_t, w1_t, w9_t, w8_t);  \
    w9_t = SHA256_EXPAND (w7_t, w2_t, wa_t, w9_t);  \
    wa_t = SHA256_EXPAND (w8_t, w3_t, wb_t, wa_t);  \
    wb_t = SHA256_EXPAND (w9_t, w4_t, wc_t, wb_t);  \
    wc_t = SHA256_EXPAND (wa_t, w5_t, wd_t, wc_t);  \
    wd_t = SHA256_EXPAND (wb_t, w6_t, we_t, wd_t);  \
    we_t = SHA256_EXPAND (wc_t, w7_t, wf_t, we_t);  \
    wf_t = SHA256_EXPAND (wd_t, w8_t, w0_t, wf_t);  \
  }

  #define ROUND_STEP(i)                                                                   \
  {                                                                                       \
    SHA256_STEP (F0, F1, a, b, c, d, e, f, g, h, w0_t, k_sha256[i +  0]); \
    SHA256_STEP (F0, F1, h, a, b, c, d, e, f, g, w1_t, k_sha256[i +  1]); \
    SHA256_STEP (F0, F1, g, h, a, b, c, d, e, f, w2_t, k_sha256[i +  2]); \
    SHA256_STEP (F0, F1, f, g, h, a, b, c, d, e, w3_t, k_sha256[i +  3]); \
    SHA256_STEP (F0, F1, e, f, g, h, a, b, c, d, w4_t, k_sha256[i +  4]); \
    SHA256_STEP (F0, F1, d, e, f, g, h, a, b, c, w5_t, k_sha256[i +  5]); \
    SHA256_STEP (F0, F1, c, d, e, f, g, h, a, b, w6_t, k_sha256[i +  6]); \
    SHA256_STEP (F0, F1, b, c, d, e, f, g, h, a, w7_t, k_sha256[i +  7]); \
    SHA256_STEP (F0, F1, a, b, c, d, e, f, g, h, w8_t, k_sha256[i +  8]); \
    SHA256_STEP (F0, F1, h, a, b, c, d, e, f, g, w9_t, k_sha256[i +  9]); \
    SHA256_STEP (F0, F1, g, h, a, b, c, d, e, f, wa_t, k_sha256[i + 10]); \
    SHA256_STEP (F0, F1, f, g, h, a, b, c, d, e, wb_t, k_sha256[i + 11]); \
    SHA256_STEP (F0, F1, e, f, g, h, a, b, c, d, wc_t, k_sha256[i + 12]); \
    SHA256_STEP (F0, F1, d, e, f, g, h, a, b, c, wd_t, k_sha256[i + 13]); \
    SHA256_STEP (F0, F1, c, d, e, f, g, h, a, b, we_t, k_sha256[i + 14]); \
    SHA256_STEP (F0, F1, b, c, d, e, f, g, h, a, wf_t, k_sha256[i + 15]); \
  }

  ROUND_STEP (0);

  ROUND_EXPAND();
  ROUND_STEP(16);

  ROUND_EXPAND();
  ROUND_STEP(32);

  ROUND_EXPAND();
  ROUND_STEP(48);

  digest[0] += a;
  digest[1] += b;
  digest[2] += c;
  digest[3] += d;
  digest[4] += e;
  digest[5] += f;
  digest[6] += g;
  digest[7] += h;
}


uint endianSwap(uint n) {
    return ( rotate(n & 0x00FF00FF, 24U) | (rotate(n, 8U) & 0x00FF00FF) );
}


unsigned int SWAP (unsigned int val)
{
    return (rotate(((val) & 0x00FF00FF), 24U) | rotate(((val) & 0xFF00FF00), 8U));
}

#define word unsigned int

static void sha256 ( uint pass_len,  const unsigned int *pass,  uint *hash) 
{                 

    int plen=pass_len/4;            
    if (mod(pass_len,4)) plen++;    
    
    unsigned int* p = hash; 
                                    
    unsigned int W[0x10]={0};   
    int loops=plen;             
    int curloop=0;              
    unsigned int State[8]={0};  
    State[0] = 0x6a09e667;      
    State[1] = 0xbb67ae85;      
    State[2] = 0x3c6ef372;      
    State[3] = 0xa54ff53a;      
    State[4] = 0x510e527f;      
    State[5] = 0x9b05688c;      
    State[6] = 0x1f83d9ab;      
    State[7] = 0x5be0cd19;      
                        
    while (loops>0)     
    {                   
        W[0x0]=0x0;     
        W[0x1]=0x0;     
        W[0x2]=0x0;     
        W[0x3]=0x0;     
        W[0x4]=0x0;     
        W[0x5]=0x0;     
        W[0x6]=0x0;     
        W[0x7]=0x0;     
        W[0x8]=0x0;     
        W[0x9]=0x0;     
        W[0xA]=0x0;     
        W[0xB]=0x0;     
        W[0xC]=0x0;     
        W[0xD]=0x0;     
        W[0xE]=0x0;     
        W[0xF]=0x0;     
                        
        for (int m=0;loops!=0 && m<16;m++)      
        {                                       
            W[m]^=SWAP(pass[m+(curloop*16)]);   
            loops--;                            
        }                            
        
                                                        
        if (loops==0 && mod(pass_len,64)!=0)    
        {                                       
            unsigned int padding=0x80<<(((pass_len+4)-((pass_len+4)/4*4))*8);   
            int v=mod(pass_len,64);         
            W[v/4]|=SWAP(padding);          
            if ((pass_len&0x3B)!=0x3B)      
            {                               
                W[0x0F]=pass_len*8;         
            }                               
        }                                   
                                        
        sha256_process2(W,State);       
        curloop++;                      
    }                                   
             

                            
    p[0]=SWAP(State[0]);    
    p[1]=SWAP(State[1]);    
    p[2]=SWAP(State[2]);    
    p[3]=SWAP(State[3]);    
    p[4]=SWAP(State[4]);    
    p[5]=SWAP(State[5]);    
    p[6]=SWAP(State[6]);    
    p[7]=SWAP(State[7]);    
  

    return;                 
}








inline void loadUintHash ( unsigned char* dest, uint* src) {


    uchar4 num0 = as_uchar4(src[0]);
    dest[3] = num0.w;
    dest[2] = num0.z;
    dest[1] = num0.y;
    dest[0] = num0.x;

    uchar4 num1 = as_uchar4(src[1]);
    dest[7] = num1.w;
    dest[6] = num1.z;
    dest[5] = num1.y;
    dest[4] = num1.x;

    uchar4 num2 = as_uchar4(src[2]);
    dest[11] = num2.w;
    dest[10] = num2.z;
    dest[9] = num2.y;
    dest[8] = num2.x;

    uchar4 num3 = as_uchar4(src[3]);
    dest[15] = num3.w;
    dest[14] = num3.z;
    dest[13] = num3.y;
    dest[12] = num3.x;

    uchar4 num4 = as_uchar4(src[4]);
    dest[19] = num4.w;
    dest[18] = num4.z;
    dest[17] = num4.y;
    dest[16] = num4.x;

    uchar4 num5 = as_uchar4(src[5]);
    dest[23] = num5.w;
    dest[22] = num5.z;
    dest[21] = num5.y;
    dest[20] = num5.x;

    uchar4 num6 = as_uchar4(src[6]);
    dest[27] = num6.w;
    dest[26] = num6.z;
    dest[25] = num6.y;
    dest[24] = num6.x;

    uchar4 num7 = as_uchar4(src[7]);
    dest[31] = num7.w;
    dest[30] = num7.z;
    dest[29] = num7.y;
    dest[28] = num7.x;



}



static inline uint CLZz(uint x)
{
    x |= x >> 1;

    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;

    x -= (x >> 1) & 0x55555555U;
    x = ((x >> 2) & 0x33333333U) + (x & 0x33333333U);
    x = ((x >> 4) + x) & 0x0f0f0f0fU;
    x += x >> 8;
    x += x >> 16;
    return 32U - (x & 0x0000003fU);
}



#define HASHOP_ADD 0
#define HASHOP_XOR 1
#define HASHOP_SHA_SINGLE 2
#define HASHOP_SHA_LOOP 3
#define HASHOP_MEMGEN 4
#define HASHOP_MEMADD 5
#define HASHOP_MEMXOR 6
#define HASHOP_MEM_SELECT 7
#define HASHOP_END 8



__kernel void dyn_hash (__global uint* byteCode, __global uint* hashResult, __global uint* hostHeader) {
    
    //int computeUnitID = get_global_id(0) * 512 + get_global_id(1);
    int computeUnitID = get_global_id(0);

    
    /*
    if (computeUnitID != 0)
        return;
        */

    /*
    for (int i = 0; i < 77; i++)
        printf("%u\n", byteCode[i]);
    return;
    */

    __global uint* hostHashResult = &hashResult[computeUnitID * 8];

    uint myMemGen[8];
    uint myHeader[20];
    uint myHashResult[8];

    uint nonce = hostHeader[19] + computeUnitID;

    for ( int i = 0; i < 20; i++)
        myHeader[i] = hostHeader[i];



    myHeader[19] = nonce;        
    

        sha256 (  80, myHeader, myHashResult );

        /*
        printf("%08X%08X%08X%08X%08X%08X%08X%08X", 
            myHashResult[0],
            myHashResult[1],
            myHashResult[2],
            myHashResult[3],
            myHashResult[4],
            myHashResult[5],
            myHashResult[6],
            myHashResult[7]
            );
            */
       

        uint linePtr = 0;
        uint done = 0;
        uint currentMemSize = 0;
        uint instruction = 0;

        uint firstMemgen = byteCode[52];
        uint secondMemgen = byteCode[65];
        uint memgenCount = 0;
        uint numToGen;

        while (done == 0) {

            if (byteCode[linePtr] == HASHOP_ADD) {
                //printf("HASHOP_ADD\n");
                linePtr++;
                for ( int i = 0; i < 8; i++) 
                    myHashResult[i] += byteCode[linePtr+i];
                linePtr += 8;
            }


            else if (byteCode[linePtr] == HASHOP_XOR) {
                //printf("HASHOP_XOR\n");
                linePtr++;
                for ( int i = 0; i < 8; i++) 
                    myHashResult[i] ^= byteCode[linePtr+i];
                linePtr += 8;
            }


            else if (byteCode[linePtr] == HASHOP_SHA_SINGLE) {
                //printf("HASHOP_SHA_SINGLE\n");

                sha256 (  32, myHashResult, myHashResult );
                linePtr++;
            }


            else if (byteCode[linePtr] == HASHOP_SHA_LOOP) {
                //printf("HASHOP_SHA_LOOP\n");
                linePtr++;
                uint loopCount = byteCode[linePtr];
                for ( int i = 0; i < loopCount; i++) {
                    sha256 (  32, myHashResult, myHashResult );
                }
                linePtr++;
            }


            else if (byteCode[linePtr] == HASHOP_MEMGEN) {
                //printf("HASHOP_MEMGEN\n");
                linePtr++;

                currentMemSize = byteCode[linePtr];
               
                if (memgenCount == 0) {
                    numToGen = firstMemgen;
                    memgenCount = 1;
                }
                else
                    numToGen = secondMemgen;
                numToGen = numToGen % currentMemSize;

                for ( int i = 0; i < currentMemSize; i++) {
                    sha256 (  32, myHashResult, myHashResult );
                    if ( i == numToGen)
                        for ( int j = 0; j < 8; j++)
                            myMemGen[j] = myHashResult[j];
                }
                
            
                linePtr++;
            }


            else if (byteCode[linePtr] == HASHOP_MEMADD) {
                //printf("HASHOP_MEMADD\n");
                linePtr++;
                
                for ( int j = 0; j < 8; j++)
                    myMemGen[j] += byteCode[linePtr+j];
            
                linePtr += 8;
            }


            else if (byteCode[linePtr] == HASHOP_MEMXOR) {
                //printf("HASHOP_MEMXOR\n");
                linePtr++;
            
                for ( int j = 0; j < 8; j++)
                    myMemGen[j] ^= byteCode[linePtr+j];
            
                linePtr += 8;
            }


            else if (byteCode[linePtr] == HASHOP_MEM_SELECT) {
                //printf("HASHOP_MEM_SELECT\n");
                linePtr++;
                for ( int j = 0; j < 8; j++)
                    myHashResult[j] = myMemGen[j];
                
                linePtr++;
            }


            else if (byteCode[linePtr] == HASHOP_END) {
                //printf("HASHOP_END\n");
                done = 1;
            }

            if (linePtr > 200)
                break;

            /*
            printf("%08X%08X%08X%08X%08X%08X%08X%08X",
                myHashResult[0],
                myHashResult[1],
                myHashResult[2],
                myHashResult[3],
                myHashResult[4],
                myHashResult[5],
                myHashResult[6],
                myHashResult[7]
                );
                */
        }


    for ( int i = 0; i < 8; i++)
        hostHashResult[i] = myHashResult[i];

}