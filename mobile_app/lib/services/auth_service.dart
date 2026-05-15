import 'package:firebase_auth/firebase_auth.dart';
import 'package:google_sign_in/google_sign_in.dart';

/// Google Sign-In (google_sign_in 7.x API) -> Firebase Auth.
///
/// CATATAN PENTING (Android): supaya `idToken` keluar dan bisa dipakai
/// Firebase, butuh `serverClientId` = OAuth **Web client ID** dari project
/// Firebase. Nilainya diisi otomatis oleh setup setelah `flutterfire
/// configure` (lihat lib/firebase_oauth.dart). Sebelum diisi, login Google
/// akan gagal dapat idToken.
import '../firebase_oauth.dart';

class AuthService {
  AuthService._();
  static final AuthService instance = AuthService._();

  final _auth = FirebaseAuth.instance;
  bool _gsInitialized = false;

  Stream<User?> get authState => _auth.authStateChanges();
  User? get currentUser => _auth.currentUser;

  Future<void> _ensureGoogleInit() async {
    if (_gsInitialized) return;
    await GoogleSignIn.instance.initialize(
      serverClientId: kGoogleServerClientId.isEmpty
          ? null
          : kGoogleServerClientId,
    );
    _gsInitialized = true;
  }

  /// Login Google interaktif. Lempar exception kalau user batal / gagal.
  Future<UserCredential> signInWithGoogle() async {
    await _ensureGoogleInit();

    final GoogleSignInAccount account =
        await GoogleSignIn.instance.authenticate(
      scopeHint: const ['email'],
    );
    final auth = account.authentication;
    final idToken = auth.idToken;
    if (idToken == null) {
      throw FirebaseAuthException(
        code: 'missing-id-token',
        message:
            'Google tidak mengembalikan idToken. Pastikan Web client ID '
            '(serverClientId) sudah benar di lib/firebase_oauth.dart.',
      );
    }

    final credential = GoogleAuthProvider.credential(idToken: idToken);
    return _auth.signInWithCredential(credential);
  }

  Future<void> signOut() async {
    try {
      await GoogleSignIn.instance.signOut();
    } catch (_) {
      // abaikan kalau plugin Google belum init
    }
    await _auth.signOut();
  }
}
