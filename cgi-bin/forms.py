import sys
import os
from urllib.parse import parse_qs
import hmac
import hashlib
import base64
import json

JWT_SECRET_KEY = "cacaprout"

def base64url_encode(data):
    return base64.urlsafe_b64encode(data).rstrip(b'=').decode('utf-8')

def create_jwt(payload, secret):
    header = {"alg": "HS256", "typ": "JWT"}
    
    header_json = json.dumps(header, separators=(',', ':')).encode('utf-8')
    payload_json = json.dumps(payload, separators=(',', ':')).encode('utf-8')
    
    unsigned_token = base64url_encode(header_json) + "." + base64url_encode(payload_json)
    
    signature = hmac.new(
        secret.encode('utf-8'),
        unsigned_token.encode('utf-8'),
        hashlib.sha256
    ).digest()
    
    return unsigned_token + "." + base64url_encode(signature)

def main():
	try:
		content_length = int(os.environ.get('CONTENT_LENGTH', 0))
		body_received = sys.stdin.read(content_length)
	except ValueError:
		body_received = None

	if not body_received or len(body_received) == 0:
		return sys.stdout.write("Status: 403\r\n\r\n<h1>No body provided</h1>")

	# extract the form data	
	parsed_form_data = parse_qs(body_received)
	first_name = parsed_form_data.get("first_name", [None])[0]
	last_name = parsed_form_data.get("last_name", [None])[0]
	username = parsed_form_data.get("username", [None])[0]

	# Verify if not valid return code 403
	if (not first_name or not last_name or not username):
		return sys.stdout.write("Status: 403\r\n\r\n<h1>Invalid form data</h1>")

	# Create JWT and return it in the headers
	jwt = create_jwt({
		"first_name": first_name,
		"last_name": last_name,
		"username": username,
	}, JWT_SECRET_KEY)
	sys.stdout.write(f"Set-cookie: token={jwt}; Max-age=3600; Path=/; HttpOnly\r\n")

	# Location et status 303 pour rediriger vers la route /me
	sys.stdout.write("Location: /me\r\n")
	sys.stdout.write("Status: 303 See Other\r\n")

	sys.stdout.write("\r\n")
	

if __name__ == "__main__":
	main()