import { Injectable } from '@angular/core';
import { HttpClient, HttpHeaders } from '@angular/common/http';
import { of, Observable } from 'rxjs';
import { map, catchError } from 'rxjs/operators';

@Injectable()
export class UsernameService {

  constructor(
    public http: HttpClient
  ) {}
  
  checkValidUsername(username: string): Observable<boolean> {
	let body = {
	  'username': username
	};
	let headers = new HttpHeaders({
	  'Content-Type' : 'application/json'
	});
	return this.http.post('/app00/check_username', JSON.stringify(body), {headers: headers}).pipe(
	  map( (data: any) => {
	    return true;
	  }), catchError ((error: any) => {
	    return of(false);
    }));
  }
  
  checkValidLoginForm(form: object): Observable<boolean> {
	let basicAuth = form['username'] + ':' + form['password']
	let headers = new HttpHeaders({
      'Authorization': 'Basic ' + btoa(basicAuth)
	});
    return this.http.get('/app00/authentication', {headers: headers, observe: 'response'}).pipe(
	  map((data: any) => {
	    if (data.headers.get('Token-Auth-App00') !== null) {
		  localStorage.setItem('token-app00', data.headers.get('Token-Auth-App00'));
		}
		return true;
	  }), catchError ((error: any) => {
	    return of(false);
    }));
  }

  checkValidRegisterForm(form: object): Observable<boolean> {
	let headers = new HttpHeaders({
 	  'Content-Type': 'application/json'
	});
	
	let user = {
	  username: form['username'],
	  password: form['password'],
	  email: form['email']
	};

    return this.http.post('/app00/registration', JSON.stringify(user), {headers: headers, observe: 'response'}).pipe(
	  map((data: any) => {
	    if (data.headers.get('Token-Auth-App00') !== null) {
		  localStorage.setItem('token-app00', data.headers.get('Token-Auth-App00'));
		}
		return true;
	  }), catchError ((error: any) => {
	    return of(false);
    }));
  }
}
