#pragma once
class call_data
{
public:
  enum CallStatus { START, READING, READING_FAILED, WRITING, FINISH };
  call_data() {}
  virtual ~call_data() {}
  virtual void process_state()=0;
  virtual void set_error()=0;
  virtual void set_state(CallStatus e)=0;
  virtual CallStatus get_state()=0;

  //virtual void  try_response() = 0;
};

