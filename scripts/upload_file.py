import sys
import os
import json
import cgi
from me import render_template

def main():
	form = cgi.FieldStorage(
		fp=sys.stdin.buffer,
		environ=os.environ,
		keep_blank_values=True
	)

	if not form:
		return sys.stdout.write("Status: 404 File Not Found\r\n")

	file_field = form["file"]
	filename = file_field.filename
	file_content = file_field.file.read()

	with open(filename, "wb") as file:
		file.write(file_content)

	# returning http reponse
	html = """<!DOCTYPE html>
				<html>
				<head>
					<meta charset="utf-8">
					<title>Profile</title>
					<style>
						* {
							margin: 0;
							padding: 0;
							box-sizing: border-box;
						}

						body {
							font-family: Arial, sans-serif;
							background-color: #333745;
							min-height: 100vh;
							display: flex;
							justify-content: center;
							align-items: center;
							padding: 20px;
						}

						h1 {
							background-color: #E0D3DE;
							color: #333745;
							padding: 20px 30px;
							border-radius: 10px 10px 10px 10px;
							font-size: 2rem;
							text-align: center;
						}
					</style>
				</head>
				<body>
					<div>
						<h1>succesfull upload</h1>
					</div>
				</body>
				</html>
			"""

	sys.stdout.write("Status: 201 File Created\r\n")
	sys.stdout.write("Content-Type: text/html\r\n")
	sys.stdout.write("\r\n")
	sys.stdout.write(html)


if __name__ == "__main__":
	main()