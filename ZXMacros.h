#ifndef _ZXMACROS_H_
#define _ZXMACROS_H_

#define A    m_Z80Processor.af.b.h
#define FL   m_Z80Processor.af.b.l
#define AF   m_Z80Processor.af.w
#define B    m_Z80Processor.bc.b.h
#define C    m_Z80Processor.bc.b.l
#define BC   m_Z80Processor.bc.w
#define D    m_Z80Processor.de.b.h
#define E    m_Z80Processor.de.b.l
#define DE   m_Z80Processor.de.w
#define H    m_Z80Processor.hl.b.h
#define L    m_Z80Processor.hl.b.l
#define HL   m_Z80Processor.hl.w
#define A_   m_Z80Processor.af_.b.h
#define F_   m_Z80Processor.af_.b.l
#define AF_  m_Z80Processor.af_.w
#define B_   m_Z80Processor.bc_.b.h
#define C_   m_Z80Processor.bc_.b.l
#define BC_  m_Z80Processor.bc_.w
#define D_   m_Z80Processor.de_.b.h
#define E_   m_Z80Processor.de_.b.l
#define DE_  m_Z80Processor.de_.w
#define H_   m_Z80Processor.hl_.b.h
#define L_   m_Z80Processor.hl_.b.l
#define HL_  m_Z80Processor.hl_.w
#define IXH  m_Z80Processor.ix.b.h
#define IXL  m_Z80Processor.ix.b.l
#define IX   m_Z80Processor.ix.w
#define IYH  m_Z80Processor.iy.b.h
#define IYL  m_Z80Processor.iy.b.l
#define IY   m_Z80Processor.iy.w
#define SPH  m_Z80Processor.sp.b.h
#define SPL  m_Z80Processor.sp.b.l
#define SP   m_Z80Processor.sp.w
#define PCH  m_Z80Processor.pc.b.h
#define PCL  m_Z80Processor.pc.b.l
#define PC   m_Z80Processor.pc.w
#define I    m_Z80Processor.i
#define R    m_Z80Processor.r
#define R7   m_Z80Processor.r7
#define IFF1 m_Z80Processor.iff1
#define IFF2 m_Z80Processor.iff2
#define IM   m_Z80Processor.im
#define IR ((m_Z80Processor.i) << 8 | (m_Z80Processor.r7 & 0x80) | (m_Z80Processor.r & 0x7F))
#define Q m_Z80Processor.q
#define FLAG_C	0x01
#define FLAG_N	0x02
#define FLAG_P	0x04
#define FLAG_V	FLAG_P
#define FLAG_3	0x08
#define FLAG_H	0x10
#define FLAG_5	0x20
#define FLAG_Z	0x40
#define FLAG_S	0x80

#define CONTENDED 0x4000
#define contendedAccess(address, time)                  { if ((address & 0xC000) == 0x4000 && m_Z80Processor.tCount > 14334 && m_Z80Processor.tCount < 57245) m_Z80Processor.tCount += m_pContendTable[m_Z80Processor.tCount - 14335]; m_Z80Processor.tCount += (time); }

#define AND(value)                                      { A &= (value); FL = FLAG_H | m_sz53pTable[A]; Q = FL; }
#define ADC(value)                                      { WORD adctemp = A + (value) + ( FL & FLAG_C ); BYTE lookup = ((A & 0x88) >> 3) | (((value) & 0x88) >> 2) | ((adctemp & 0x88) >> 1); \
                                                          A = adctemp; FL = (adctemp & 0x100 ? FLAG_C : 0) | m_halfcarryAddTable[lookup & 0x07] | m_overflowAddTable[lookup >> 4] | m_sz53Table[A]; \
                                                          Q = FL; }
#define ADC16(value)                                    { DWORD add16temp= HL + (value) + (FL & FLAG_C); BYTE lookup = ((HL & 0x8800) >> 11) | (((value) & 0x8800) >> 10) | \
                                                          ((add16temp & 0x8800) >>  9); m_Z80Processor.memptr.w = HL + 1; HL = add16temp; FL= (add16temp & 0x10000 ? FLAG_C : 0) | \
                                                          m_overflowAddTable[lookup >> 4] | (H & (FLAG_3 | FLAG_5 | FLAG_S)) | m_halfcarryAddTable[lookup&0x07] | (HL ? 0 : FLAG_Z); Q = FL; }
#define ADD(value)                                      { WORD addtemp = A + (value); BYTE lookup = ((A & 0x88) >> 3) | (((value) & 0x88) >> 2 ) | ((addtemp & 0x88) >> 1); A = addtemp; \
                                                          FL = (addtemp & 0x100 ? FLAG_C : 0) | m_halfcarryAddTable[lookup & 0x07] | m_overflowAddTable[lookup >> 4] | m_sz53Table[A]; Q = FL; }
#define ADD16(value1, value2)                           { DWORD add16temp = (value1) + (value2); BYTE lookup = (((value1) & 0x0800) >> 11) | (((value2) & 0x0800) >> 10 )| \
			                                              ((add16temp & 0x0800) >>  9); m_Z80Processor.memptr.w = (value1) + 1; (value1) = add16temp; FL= (FL& (FLAG_V | FLAG_Z | FLAG_S)) | \
                                                          (add16temp & 0x10000 ? FLAG_C : 0) | ((add16temp >> 8) & (FLAG_3 | FLAG_5)) | m_halfcarryAddTable[lookup]; Q = FL; }
#define BIT_REG(bit, value)                             { FL = (FL & FLAG_C) | FLAG_H | (value & (FLAG_3 | FLAG_5)); if (!((value) & (0x01 << (bit)))) FL |= FLAG_P | FLAG_Z; \
                                                          if ((bit) == 7 && (value) & 0x80) FL |= FLAG_S; Q = FL; }
#define BIT_MEMPTR(bit, value)                          { FL = (FL & FLAG_C) | FLAG_H | (m_Z80Processor.memptr.b.h & (FLAG_3 | FLAG_5)); if (!((value) & (0x01 << (bit)))) FL|= FLAG_P | FLAG_Z; \
                                                          if ((bit) == 7 && (value) & 0x80) FL|= FLAG_S; Q = FL; }  
#define CALL()                                          { contendedAccess(PC, 1); PC++; PUSH16(PCL, PCH); PC = m_Z80Processor.memptr.w; }
#define CP(value)                                       { WORD cptemp = A - value; BYTE lookup = ((A & 0x88) >> 3) | (((value) & 0x88) >> 2) | ((cptemp & 0x88) >> 1); \
                                                          FL =  (cptemp & 0x100 ? FLAG_C : (cptemp ? 0 : FLAG_Z)) | FLAG_N | m_halfcarrySubTable[lookup & 0x07] | m_overflowSubTable[lookup >> 4] | \
                                                          (value & (FLAG_3 | FLAG_5)) | (cptemp & FLAG_S); Q = FL; }
#define DEC8(value)                                     { FL = (FL & FLAG_C) | ((value) & 0x0f ? 0 : FLAG_H) | FLAG_N; (value)--; FL|= ((value) == 0x7f ? FLAG_V : 0) | m_sz53Table[value]; Q = FL; }
#define INP(reg, port)                                  { m_Z80Processor.memptr.w = port + 1; (reg) = readPort((port)); FL= (FL & FLAG_C) | m_sz53pTable[(reg)]; Q = FL; }
#define INC8(value)                                     { (value)++; FL= (FL& FLAG_C) | ((value)==0x80 ? FLAG_V : 0) | ((value)&0x0f ? 0 : FLAG_H) | m_sz53Table[(value)]; Q = FL; }
#define LD16_NNRR(regl, regh)                           { WORD ldtemp; ldtemp = readMem(PC++); ldtemp |= readMem(PC++) << 8; writeMem(ldtemp++, (regl)); m_Z80Processor.memptr.w = ldtemp;\
                                                          writeMem(ldtemp,(regh)); break; }
#define LD16_RRNN(regl, regh)                           { WORD ldtemp; ldtemp = readMem(PC++); ldtemp |= readMem(PC++) << 8; (regl) = readMem(ldtemp++); m_Z80Processor.memptr.w=ldtemp;\
                                                          (regh) = readMem(ldtemp); break; }
#define JP()                                            { PC = m_Z80Processor.memptr.w; }
#define JR()                                            { OFFSET jrtemp = readMem(PC); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1); contendedAccess(PC, 1);\
                                                          PC += jrtemp; PC++; m_Z80Processor.memptr.w = PC; }
#define OR(value)                                       { A |= (value); FL= m_sz53pTable[A]; Q = FL; }
#define POP16(regl, regh)                               { (regl) = readMem(SP++); (regh) = readMem(SP++); }
#define PUSH16(regl, regh)                              { writeMem(--SP, (regh)); writeMem(--SP, (regl)); }
#define RET()                                           { POP16(PCL, PCH); m_Z80Processor.memptr.w = PC; }
#define RL(value)                                       { BYTE rltemp = (value); (value) = ((value) << 1) | (FL& FLAG_C); FL= (rltemp >> 7) | m_sz53pTable[(value)]; Q = FL; }
#define RLC(value)                                      { (value) = ((value) << 1) | ((value)>>7); FL= ((value) & FLAG_C) | m_sz53pTable[(value)]; Q = FL; }
#define RR(value)                                       { BYTE rrtemp = (value); (value) = ((value) >> 1 ) | (FL<< 7); FL= (rrtemp & FLAG_C) | m_sz53pTable[(value)]; Q = FL; }
#define RRC(value)                                      { FL= (value) & FLAG_C; (value) = ((value) >> 1) | ((value) << 7); FL|= m_sz53pTable[(value)]; Q = FL; }
#define RST(value)                                      { PUSH16(PCL, PCH); PC = (value); m_Z80Processor.memptr.w = PC; }
#define SBC(value)                                      { WORD sbctemp = A - (value) - (FL& FLAG_C); BYTE lookup = ((A & 0x88) >> 3) | (((value) & 0x88) >> 2) | ((sbctemp & 0x88) >> 1); \
                                                          A = sbctemp; FL= (sbctemp & 0x100 ? FLAG_C : 0) | FLAG_N | m_halfcarrySubTable[lookup & 0x07] | m_overflowSubTable[lookup >> 4] | \
                                                          m_sz53Table[A]; Q = FL; }
#define SBC16(value)                                    { DWORD sub16temp = HL - (value) - (FL& FLAG_C); BYTE lookup = ((HL & 0x8800) >> 11) | (((value) & 0x8800) >> 10) | \
                                                          ((sub16temp & 0x8800) >>  9); m_Z80Processor.memptr.w = HL + 1; HL = sub16temp; FL= (sub16temp & 0x10000 ? FLAG_C : 0) | FLAG_N | \
                                                          m_overflowSubTable[lookup >> 4] | (H & (FLAG_3 | FLAG_5 | FLAG_S)) | m_halfcarrySubTable[lookup&0x07] | (HL ? 0 : FLAG_Z); Q = FL; }
#define SLA(value)                                      { FL= (value) >> 7; (value) <<= 1; FL|= m_sz53pTable[(value)]; Q = FL; }
#define SLL(value)                                      { FL= (value) >> 7;  (value) = ((value) << 1) | 0x01; FL|= m_sz53pTable[(value)]; Q = FL; }
#define SRA(value)                                      { FL= (value) & FLAG_C; (value) = ((value) & 0x80) | ((value) >> 1); FL|= m_sz53pTable[(value)]; Q = FL; }
#define SRL(value)                                      { FL= (value) & FLAG_C; (value) >>= 1; FL|= m_sz53pTable[(value)]; Q = FL; }
#define SUB(value)                                      { WORD subtemp = A - (value); BYTE lookup = ((A & 0x88) >> 3 ) | (((value) & 0x88) >> 2) | ((subtemp & 0x88) >> 1); A = subtemp; \
                                                          FL= (subtemp & 0x100 ? FLAG_C : 0) | FLAG_N | m_halfcarrySubTable[lookup & 0x07] | m_overflowSubTable[lookup >> 4] |m_sz53Table[A]; Q = FL; }
#define XOR(value)                                      { A ^= (value); FL= m_sz53pTable[A]; Q = FL; }
#endif // !_ZXMACROS_H_
