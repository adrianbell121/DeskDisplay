"""Current weather for scene effects, using Open-Meteo with a ten-minute cache."""
import json
import math
import threading
import time
from urllib.parse import urlencode
from urllib.request import Request,urlopen

_cache={}
_lock=threading.Lock()

def condition(code):
    if code==0:return 'sunny'
    if code in (1,2):return 'partly-cloudy'
    if code in (3,45,48):return 'cloudy'
    if code in (71,73,75,77,85,86):return 'snowy'
    if code in (95,96,99):return 'storm'
    if code in (51,53,55,56,57,61,63,65,66,67,80,81,82):return 'rainy'
    return 'cloudy'

def fetch_json(url):
    with urlopen(Request(url,headers={'User-Agent':'DeskDisplay/1.0'}),timeout=15) as response:
        return json.load(response)

def current_weather(location):
    if not isinstance(location,str) or not 2<=len(location.strip())<=100:raise ValueError('Enter a city and state, or city and country.')
    key=location.strip().casefold()
    with _lock:
        cached=_cache.get(key)
        if cached and time.time()-cached['fetchedAt']<600:return dict(cached)
    if key in ('boston, ma','boston ma','boston, massachusetts'):
        place={'name':'Boston','admin1':'Massachusetts','latitude':42.3601,'longitude':-71.0589}
    else:
        places=fetch_json('https://geocoding-api.open-meteo.com/v1/search?'+urlencode({'name':location.strip(),'count':1,'language':'en','format':'json'})).get('results',[])
        if not places:raise ValueError('Location not found. Include a state or country.')
        place=places[0]
    params={'latitude':place['latitude'],'longitude':place['longitude'],'current':'temperature_2m,weather_code,is_day','temperature_unit':'fahrenheit','timezone':'auto','forecast_days':1}
    raw=fetch_json('https://api.open-meteo.com/v1/forecast?'+urlencode(params));now=raw['current']
    temperature=float(now['temperature_2m']);code=int(now['weather_code'])
    if not math.isfinite(temperature):raise ValueError('Weather provider returned an invalid temperature.')
    result={'location':place['name']+', '+place.get('admin1',place.get('country','')),'latitude':place['latitude'],'longitude':place['longitude'],'condition':condition(code),'weatherCode':code,'tempF':temperature,'isDay':bool(now.get('is_day',1)),'observedAt':now['time'],'timezone':raw.get('timezone'),'fetchedAt':time.time(),'source':'Open-Meteo','sourceUrl':'https://open-meteo.com/'}
    with _lock:_cache[key]=result
    return dict(result)
