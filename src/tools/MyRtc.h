#ifndef __MYRTC_H
#define __MYRTC_H

class MyRtcTime {
private:
  time_t m_unixTime;
public:
  MyRtcTime(time_t unixTime);
  String getTimestampText(void) const;
  String getDateText(void) const;
  time_t getUnixTime(void) const;
  bool isValid();
};

class CMyRtc {
public:
  void start(void);
  MyRtcTime getTime();
  bool setTime(MyRtcTime const &);
};

extern CMyRtc MyRtc;

#endif