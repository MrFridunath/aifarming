import { Injectable } from '@angular/core';
import { UsernameService } from '../services/username.service';
import { of, Observable } from 'rxjs';
import { switchMap, map, take, debounceTime, catchError } from 'rxjs/operators';
import { AsyncValidatorFn, AbstractControl } from '@angular/forms';

@Injectable()
export class UsernameValidator {
  constructor(
    public userService: UsernameService
  ) {}

  existingUsername(): AsyncValidatorFn {
    return (control: AbstractControl): Promise<{ [key: string]: any } | null> | Observable<{ [key: string]: any } | null> => {
	  if (!control.valueChanges) {
        return of(null)
      } else {
        return control.valueChanges.pipe(
		  debounceTime(1000),
		  take(1),
		  switchMap(value => this.userService.checkValidUsername(value)),
		  map(isValid => (isValid ? null : { 'invalid': true })),
          catchError(() => null)
      )} 
    }
  }
  
  notExistingUsername(): AsyncValidatorFn {
    return (control: AbstractControl): Promise<{ [key: string]: any } | null> | Observable<{ [key: string]: any } | null> => {
	  if (!control.valueChanges) {
        return of(null)
      } else {
        return control.valueChanges.pipe(
		  debounceTime(1000),
		  take(1),
		  switchMap(value => this.userService.checkValidUsername(value)),
		  map(isValid => (isValid ? { 'invalid': true } : null)),
          catchError(() => null)
      )} 
    }
  }
}
