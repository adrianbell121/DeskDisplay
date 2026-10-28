import unittest
from widget_providers import sample_data,normalize_data,google_events_preview,providers

class ProviderTests(unittest.TestCase):
    def test_current_and_five_day_contract(self):
        data=sample_data('2026-10-28')
        self.assertEqual(len(data['days']),5)
        self.assertEqual(len(data['hourly']),120)
        self.assertEqual(data['days'][-1]['date'],'2026-11-01')
        self.assertEqual(normalize_data(data),data)
        self.assertEqual(providers()[1]['status'],'not-connected')

    def test_probability_temperature_and_order_validation(self):
        for value in [-1,101,float('nan'),'50',True]:
            data=sample_data('2026-10-28');data['current']['precipProbability']=value
            with self.assertRaises(ValueError):normalize_data(data)
        data=sample_data('2026-10-28');data['hourly'][2]['hour']=0
        with self.assertRaises(ValueError):normalize_data(data)

    def test_google_adapter_all_day_cancelled_and_window(self):
        records=[{'id':'a','summary':'Meeting','start':{'dateTime':'2026-10-28T09:00:00-04:00'}},{'id':'b','summary':'Tomorrow','start':{'date':'2026-10-29'}},{'id':'c','status':'cancelled','start':{'date':'2026-10-28'}},{'id':'d','start':{'date':'2026-11-01'}}]
        events=google_events_preview(records,'2026-10-28')
        self.assertEqual([e['id'] for e in events],['a','b'])
        self.assertTrue(events[1]['allDay'])
        self.assertEqual(google_events_preview([],'2026-10-28'),[])

if __name__=='__main__':unittest.main()
