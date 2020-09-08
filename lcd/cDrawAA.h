// cDrawAA - anti aliased drawing
#pragma once
#include "cPointRect.h"

class cDrawAA {
public:
  cDrawAA();
  virtual ~cDrawAA();

  void moveTo (int32_t x, int32_t y);
  void lineTo (int32_t x, int32_t y);
  void render (const uint16_t colour, bool fillNonZero, uint16_t* frameBuf, uint16_t width, uint16_t height);

private:
  //{{{
  struct sCell {
  public:
    //{{{
    void set (int16_t x, int16_t y, int c, int a) {

      mPackedCoord = (y << 16) + x;
      mCoverage = c;
      mArea = a;
      }
    //}}}
    //{{{
    void setCoverage (int32_t c, int32_t a) {

      mCoverage = c;
      mArea = a;
      }
    //}}}
    //{{{
    void addCoverage (int32_t c, int32_t a) {

      mCoverage += c;
      mArea += a;
      }
    //}}}

    int32_t mPackedCoord;
    int32_t mCoverage;
    int32_t mArea;
    };
  //}}}
  //{{{
  class cScanLine {
  public:
    //{{{
    class iterator {
    public:
      iterator (const cScanLine& scanLine) :
        mCoverage(scanLine.mCoverage), mCurCount(scanLine.mCounts), mCurStartPtr(scanLine.mStartPtrs) {}

      int next() {
        ++mCurCount;
        ++mCurStartPtr;
        return int(*mCurStartPtr - mCoverage);
        }

      int getNumPix() const { return int(*mCurCount); }
      const uint8_t* getCoverage() const { return *mCurStartPtr; }

    private:
      const uint8_t* mCoverage;
      const uint16_t* mCurCount;
      const uint8_t* const* mCurStartPtr;
      };
    //}}}
    friend class iterator;

    cScanLine() {}
    ~cScanLine();

    int16_t getY() const { return mLastY; }
    int16_t getBaseX() const { return mMinx;  }
    uint16_t getNumSpans() const { return mNumSpans; }
    int isReady (int16_t y) const { return mNumSpans && (y ^ mLastY); }

    void initSpans();
    void init (int16_t minx, int16_t maxx);
    void addSpan (int16_t x, int16_t y, uint16_t num, uint16_t coverage);

  private:
    int16_t mMinx = 0;
    uint16_t mMaxlen = 0;
    int16_t mLastX = 0x7FFF;
    int16_t mLastY = 0x7FFF;

    uint8_t* mCoverage = nullptr;

    uint8_t** mStartPtrs = nullptr;
    uint8_t** mCurStartPtr = nullptr;

    uint16_t* mCounts = nullptr;
    uint16_t* mCurCount = nullptr;

    uint16_t mNumSpans = 0;
    };
  //}}}

  void init();

  // gets
  int32_t getMinx() const { return mMinx; }
  int32_t getMiny() const { return mMiny; }
  int32_t getMaxx() const { return mMaxx; }
  int32_t getMaxy() const { return mMaxy; }
  uint16_t getNumCells() const { return mNumCells; }
  const sCell* const* getSortedCells();

  void addCurCell();
  void setCurCell (int16_t x, int16_t y);
  void swapCells (sCell** a, sCell** b);
  void sortCells();
  void qsortCells (sCell** start, unsigned numCells);

  void addScanLine (int32_t ey, int32_t x1, int32_t y1, int32_t x2, int32_t y2);
  void addLine (int32_t x1, int32_t y1, int32_t x2, int32_t y2);

  void renderScanLine (const uint16_t colour, uint16_t* frameBuf, uint16_t width, uint16_t height);

  static uint8_t calcAlpha (int area, bool fillNonZero);

  //{{{  vars
  uint16_t mNumCellsInBlock = 0;
  uint16_t mNumBlockOfCells = 0;
  uint16_t mNumSortedCells = 0;
  sCell** mBlockOfCells = nullptr;
  sCell** mSortedCells = nullptr;

  uint16_t mNumCells;
  sCell mCurCell;
  sCell* mCurCellPtr = nullptr;

  int32_t mCurx = 0;
  int32_t mCury = 0;
  int32_t mClosex = 0;
  int32_t mClosey = 0;

  int32_t mMinx;
  int32_t mMiny;
  int32_t mMaxx;
  int32_t mMaxy;

  bool mClosed;
  bool mSortRequired;

  uint8_t mGamma[256];
  cScanLine* mScanLine = nullptr;
  //}}}
  };
