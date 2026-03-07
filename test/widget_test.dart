import 'package:flutter_test/flutter_test.dart';
import 'package:pifridge_mobile_app/main.dart';

void main() {
  testWidgets('PiFridge Dashboard smoke test', (WidgetTester tester) async {
    // 1. Tell the tester to load your PiFridge app
    await tester.pumpWidget(const PiFridgeApp());

    // 2. Check if the title "PiFridge" is actually on the screen
    expect(find.textContaining('PiFridge'), findsOneWidget);

    // 3. Verify that the "Add Item" button is visible
    expect(find.text('Add Item'), findsOneWidget);
  });
}