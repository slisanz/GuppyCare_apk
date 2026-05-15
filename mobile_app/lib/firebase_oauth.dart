/// OAuth **Web client ID** project Firebase `guppycare`.
///
/// Dipakai sebagai `serverClientId` google_sign_in di Android supaya
/// menghasilkan `idToken` yang valid untuk Firebase Auth.
///
/// Cara isi (otomatis oleh setup setelah `flutterfire configure`):
///   ambil dari `android/app/google-services.json` -> objek `oauth_client`
///   yang `client_type` == 3 (web), field `client_id`.
///
/// Selama masih kosong, login Google akan gagal mendapatkan idToken.
///
/// Diisi 2026-05-15 dari google-services.json (oauth_client client_type 3).
const String kGoogleServerClientId =
    '914277089837-9lr6o3o4uh6qrr4dmtdk5r8llvqqrgfs.apps.googleusercontent.com';
