"""Provider contracts and samples. No network, OAuth, serial, or device access."""
from datetime import date, datetime, timedelta
import math

CONDITIONS = ('sunny', 'cloudy', 'rainy', 'snowy')

def number(value, low, high, field):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) or not low <= value <= high:
        raise ValueError(f'{field} must be a number in {low}..{high}')
    return value

def weather_item(raw):
    if not isinstance(raw, dict) or raw.get('condition') not in CONDITIONS:
        raise ValueError('Condition must be sunny, cloudy, rainy, or snowy')
    return dict(condition=raw['condition'], tempF=number(raw.get('tempF'), -150, 150, 'tempF'), precipProbability=number(raw.get('precipProbability'), 0, 100, 'precipProbability'))

def normalize_data(raw):
    if not isinstance(raw, dict): raise ValueError('Expected a provider data object')
    reference = date.fromisoformat(str(raw.get('referenceDate',''))).isoformat()
    days = raw.get('days', [])
    if not isinstance(days, list) or len(days) != 5: raise ValueError('Forecast needs exactly five days')
    forecast = [dict(weather_item(d), date=date.fromisoformat(d['date']).isoformat()) for d in days]
    if [d['date'] for d in forecast] != [(date.fromisoformat(reference)+timedelta(days=i)).isoformat() for i in range(5)]: raise ValueError('Forecast must cover five consecutive days from referenceDate')
    hourly = raw.get('hourly', [])
    if not isinstance(hourly, list) or not 2 <= len(hourly) <= 240: raise ValueError('Provide 2–240 precipitation samples')
    hourly = [dict(hour=number(p.get('hour'),0,240,'hour'),precipProbability=number(p.get('precipProbability'),0,100,'precipProbability')) for p in hourly]
    if any(a['hour'] >= b['hour'] for a,b in zip(hourly,hourly[1:])): raise ValueError('Hourly samples must be in increasing hour order')
    events = raw.get('events', [])
    if not isinstance(events,list) or len(events)>100: raise ValueError('Provide at most 100 calendar events')
    normalized=[]
    for e in events:
        if not isinstance(e,dict): raise ValueError('Invalid calendar event')
        start=str(e.get('start',''));datetime.fromisoformat(start.replace('Z','+00:00'))
        normalized.append(dict(id=str(e.get('id',''))[:80],title=str(e.get('title','Untitled'))[:120],start=start,allDay=e.get('allDay') is True))
    return dict(mode='sample' if raw.get('mode')=='sample' else 'preview',label=str(raw.get('label','Preview data'))[:80],referenceDate=reference,current=weather_item(raw.get('current')),days=forecast,hourly=hourly,events=sorted(normalized,key=lambda e:e['start']))

def sample_data(reference=None):
    ref=date.fromisoformat(reference) if reference else date.today()
    day=lambda i:(ref+timedelta(days=i)).isoformat()
    return normalize_data(dict(mode='sample',label='Sample town',referenceDate=day(0),current=dict(condition='cloudy',tempF=68,precipProbability=25),
        days=[dict(date=day(i),condition=c,tempF=[68,65,59,34,61][i],precipProbability=[25,45,85,65,15][i]) for i,c in enumerate(CONDITIONS+('sunny',))],
        hourly=[dict(hour=i,precipProbability=round(max(0,min(100,45+34*math.sin(i*.07-1.5)+14*math.sin(i*.23))))) for i in range(120)],
        events=[dict(id='sample-1',title='Design review',start=day(0)+'T14:00:00',allDay=False),dict(id='sample-2',title='Pick up parcel',start=day(0)+'T17:30:00',allDay=False),dict(id='sample-3',title='Team standup',start=day(1)+'T09:30:00',allDay=False)]))

def google_events_preview(events,reference):
    """Normalize an Events.list-shaped payload; does not request account access."""
    if not isinstance(events,list) or len(events)>100: raise ValueError('Expected at most 100 Google Calendar event records')
    allowed={reference,(date.fromisoformat(reference)+timedelta(days=1)).isoformat()};result=[]
    for event in events:
        if not isinstance(event,dict): raise ValueError('Invalid Google event record')
        if event.get('status')=='cancelled': continue
        start=event.get('start',{});value=start.get('dateTime') or start.get('date')
        if not value: continue
        if value[:10] in allowed:
            datetime.fromisoformat(value.replace('Z','+00:00'))
            result.append(dict(id=str(event.get('id',''))[:80],title=str(event.get('summary','Untitled'))[:120],start=value,allDay='date' in start))
    return sorted(result,key=lambda e:e['start'])

def providers():
    return [dict(id='weather',status='sample',bindings=['weather.current','weather.forecast','weather.tempF','weather.precipProbability'],units='F',description='Current conditions, five daily forecasts, and hourly precipitation probabilities.'),dict(id='google-calendar',status='not-connected',bindings=['calendar.events'],description='Today/tomorrow adapter ready. OAuth and automatic synchronization are future work.')]
