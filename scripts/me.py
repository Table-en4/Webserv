import sys
import base64
import json
import os
from urllib.parse import parse_qs

JWT_SECRET_KEY = "cacaprout"

def decode_jwt_payload(token):
    """
    Décode le payload d'un JWT sans vérifier la signature.
    Utile pour extraire les données utilisateur (name, uid, etc.)
    """
    try:
        # 1. On sépare les trois parties du token
        segments = token.split('.')
        if len(segments) != 3:
            raise ValueError("Format de token invalide")

        payload_b64 = segments[1]

        # 2. Correction du padding Base64
        # Le standard JWT retire les '=' à la fin, mais Python en a besoin
        missing_padding = len(payload_b64) % 4
        if missing_padding:
            payload_b64 += '=' * (4 - missing_padding)

        # 3. Décodage Base64 URL-safe vers String UTF-8
        decoded_bytes = base64.urlsafe_b64decode(payload_b64)
        payload_str = decoded_bytes.decode('utf-8')

        # 4. Conversion du JSON en dictionnaire Python
        return json.loads(payload_str)

    except Exception as e:
        print(f"Erreur de décodage : {e}")
        return None

def main():
	# Extract cookie from environment
	cookie = os.environ.get("COOKIE", None) # token=eyXYZ....
	
	print(cookie)

	# return 403 if no cookie
	if not cookie:
		return sys.stdout.write("Status: 403 Not authenticated\r\n\r\n<h1>Not auth</h1>")
	
	# Check JWT validity
	parsed_form_data = parse_qs(cookie)
	jwt = parsed_form_data.get("token", [None])[0]

	if not jwt:
		return sys.stdout.write("Status: 403 No JWt\r\n\r\n<h1>Not auth</h1>")

	payload = decode_jwt_payload(jwt)
	if not payload:
		return sys.stdout.write("Status: 403 Invalid JWT\r\n\r\n<h1>Not auth</h1>")
		
	first_name = payload.get("first_name")
	last_name = payload.get("last_name")
	username = payload.get("username")



	html = f'''
	<!DOCTYPE html>
<html>
<head>
	<meta charset='utf-8'>
	<meta http-equiv='X-UA-Compatible' content='IE=edge'>
	<title>Page Title</title>
	<meta name='viewport' content='width=device-width, initial-scale=1'>
	<link rel='stylesheet' type='text/css' media='screen' href='main.css'>
	<script src='main.js'></script>
</head>
<body>
	<h1>Hello {username} !</h1>
	<ul>
		<li>first name: {first_name}</li>
		<li>last_name: {last_name}</li>
	</ul>

</body>
</html>
'''

	sys.stdout.write("Content-type: text/html\r\n")
	sys.stdout.write("\r\n")

	sys.stdout.write(html)
	return 

if __name__ == "__main__":
	main()