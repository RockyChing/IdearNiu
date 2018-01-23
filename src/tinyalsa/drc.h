#ifndef DRC_H_
#define DRC_H_
/*
function :drcinit
init function
input:
  fs:采样率
return :
   无返回值
*/
void drcinit(int fs);
/*
function :drcdec
drc function
input:
  xout			:处理数据首地址；
  samplenum	:处理sample数，单通路samplenum = 处理数据总长度/2;双声道samplenum = 处理数据总长度/4;
  nch       :通道数
return :
   无返回值
*/
void drcdec(short* xout,int samplenum, int nch);

#endif//DRC_H_
