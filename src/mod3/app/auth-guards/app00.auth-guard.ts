import { Injectable } from '@angular/core';
import { Router, CanActivate, CanActivateChild, CanLoad } from '@angular/router';
import { HttpClient, HttpHeaders } from '@angular/common/http';
import { of, Observable } from 'rxjs';
import { map, catchError } from 'rxjs/operators';

@Injectable()
export class App00AuthGuard implements CanActivate, CanActivateChild, CanLoad {

  constructor(
	public router: Router,
	public http: HttpClient,
  ) {}

  canActivate(): Observable<boolean> {
	if (localStorage.getItem('token-app00') === null) {
	  this.router.navigate(['/ia-farming/login']);
	  return of(false);
	}
	
	let headers = new HttpHeaders({
	    'Authorization': 'Bearer ' + localStorage.getItem('token-app00')
	});
	return this.http.get('/app00/authorization', {headers: headers, observe: 'response'}).pipe(
	  map( (data: any) => {
	    if (data.headers.get('Token-Auth-App00') !== null) {
		  localStorage.setItem('token-app00', data.headers.get('Token-Auth-App00'));
	    }
	    return true;
	  }), catchError ((error: any) => {
 	    this.router.navigate(['/ia-farming/login']);
	    return of(false);
    }));
  }  
  
  canLoad(): Observable<boolean> {
	if (localStorage.getItem('token-app00') === null) {
	  this.router.navigate(['/ia-farming/login']);
	  return of(false);
	}
	
	let headers = new HttpHeaders({
	    'Authorization': 'Bearer ' + localStorage.getItem('token-app00')
	});
	return this.http.get('/app00/authorization', {headers: headers, observe: 'response'}).pipe(
	  map( (data: any) => {
	    if (data.headers.get('Token-Auth-App00') !== null) {
		  localStorage.setItem('token-app00', data.headers.get('Token-Auth-App00'));
	    }
	    return true;
	  }), catchError ((error: any) => {
 	    this.router.navigate(['/ia-farming/login']);
	    return of(false);
    }));
  }
  
  canActivateChild(): Observable<boolean> {
	if (localStorage.getItem('token-app00') === null) {
	  this.router.navigate(['/ia-farming/login']);
	  return of(false);
	}
	
	let headers = new HttpHeaders({
	    'Authorization': 'Bearer ' + localStorage.getItem('token-app00')
	});
	return this.http.get('/app00/authorization', {headers: headers, observe: 'response'}).pipe(
	  map( (data: any) => {
	    if (data.headers.get('Token-Auth-App00') !== null) {
		  localStorage.setItem('token-app00', data.headers.get('Token-Auth-App00'));
	    }
	    return true;
	  }), catchError ((error: any) => {
 	    this.router.navigate(['/ia-farming/login']);
	    return of(false);
    }));
  }
}