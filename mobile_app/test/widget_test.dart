// Unit test logika model (tanpa Firebase, aman dijalankan di CI/lokal).
import 'package:flutter_test/flutter_test.dart';
import 'package:mobile_app/models/device_state.dart';

void main() {
  group('FeedSchedule.displayTime', () {
    test('pagi -> AM', () {
      expect(
        const FeedSchedule(time: '08:00', enabled: true).displayTime,
        '08:00 AM',
      );
    });
    test('siang 12 -> 12 PM', () {
      expect(
        const FeedSchedule(time: '12:00', enabled: false).displayTime,
        '12:00 PM',
      );
    });
    test('sore 17:30 -> 05:30 PM', () {
      expect(
        const FeedSchedule(time: '17:30', enabled: true).displayTime,
        '05:30 PM',
      );
    });
    test('tengah malam 00:05 -> 12:05 AM', () {
      expect(
        const FeedSchedule(time: '00:05', enabled: true).displayTime,
        '12:05 AM',
      );
    });
  });

  group('DeviceState.fromSnapshot', () {
    test('schedules map "0/1/2" + portion string ke-parse', () {
      final d = DeviceState.fromSnapshot({
        'schedules': {
          '0': {'time': '09:15', 'enabled': true},
          '1': {'time': '13:00', 'enabled': false},
          '2': {'time': '18:45', 'enabled': true},
        },
        'portion_mg': '25',
        'tds_ppm': 142.7,
        'tds_alert': false,
        'online': true,
      });
      expect(d.schedules[0].time, '09:15');
      expect(d.schedules[1].enabled, false);
      expect(d.portionMg, 25);
      expect(d.tdsPpm, closeTo(142.7, 0.01));
      expect(d.online, true);
    });

    test('snapshot null -> default aman', () {
      final d = DeviceState.fromSnapshot(null);
      expect(d.schedules.length, 3);
      expect(d.portionMg, 10);
      expect(d.tdsAlert, false);
    });
  });
}
