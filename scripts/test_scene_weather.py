import unittest
from unittest.mock import patch
import scene_weather as weather

class WeatherTests(unittest.TestCase):
 def setUp(self):weather._cache.clear()
 def test_codes(self):
  for code,expected in [(0,'sunny'),(2,'partly-cloudy'),(3,'cloudy'),(45,'cloudy'),(61,'rainy'),(75,'snowy'),(95,'storm'),(99,'storm')]:self.assertEqual(weather.condition(code),expected)
 def test_boston_cache_units_and_failure(self):
  payload={'current':{'temperature_2m':57.2,'weather_code':95,'is_day':1,'time':'2026-10-28T14:00'},'timezone':'America/New_York'}
  with patch.object(weather,'fetch_json',return_value=payload) as get:
   data=weather.current_weather('Boston, MA');self.assertEqual(data['condition'],'storm');self.assertEqual(data['tempF'],57.2);self.assertEqual(weather.current_weather('Boston, MA'),data);get.assert_called_once();self.assertIn('fahrenheit',get.call_args.args[0])
  with patch.object(weather,'fetch_json',side_effect=OSError('offline')):
   with self.assertRaises(OSError):weather.current_weather('New York, NY')
 def test_lookup_and_invalid_locations(self):
  with patch.object(weather,'fetch_json',return_value={'results':[]}):
   with self.assertRaises(ValueError):weather.current_weather('No such city')
  for raw in ['',None,'x'*101]:
   with self.assertRaises(ValueError):weather.current_weather(raw)
