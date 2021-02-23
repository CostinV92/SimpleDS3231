#include <SimpleDS3231.h>

/*
 * Create SimpleDS3231 object.
 */
SimpleDS3231 rtc;

int last_time;

void setup()
{
  Serial.begin(9600);

  /*
   * Initiate SimpleDS3231 object.
   */
  rtc.begin();

  /*
   * Set time. rtc.set_time(hour, minute, second, is_12_hour_format, is_pm)
   */
  rtc.set_time(23, 5, 25, false, false);

  /*
   * Set date. rtc.set_date(day_of_month, month, year)
   */
  rtc.set_date(23, 2, 2021);
}

void loop()
{
  /*
   * Print time and date every second.
   */
   Serial.println(rtc.get_time_str());
   Serial.println(rtc.get_date_str());
   while(millis() - last_time < 1000) {}
   last_time = millis();
}