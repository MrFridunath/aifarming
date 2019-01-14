import { Component, OnInit, OnDestroy } from '@angular/core';
import { FormBuilder, FormGroup, FormControl, Validators } from '@angular/forms';
import { trigger,state, animate, transition, style,} from '@angular/animations';
import { HttpClient, HttpHeaders } from '@angular/common/http';
import { Router } from '@angular/router';
import { UsernameValidator } from '../validators/username.validator';
import { matchPasswordValidator } from '../validators/password.validator';
import { UsernameService } from '../services/username.service';
import { Subscription } from 'rxjs';

@Component({
  selector: 'app-app00-login',
  templateUrl: './login.component.html',
  styleUrls: ['./login.component.css'],
  animations: [
    trigger('inputValidation', [
	  transition(':enter', [ style({ opacity: 0 }), animate('1s', style({ opacity: 1 })) ]),
      transition(':leave', [ animate('0.5s', style({ opacity: 0 })) ])
    ])
  ]
})
export class App00LoginComponent implements OnInit, OnDestroy {
  
  loginForm: FormGroup;
  registerForm: FormGroup;

  registration: boolean = false;
  
  invalidLoginForm: boolean = false;
  usernameSubscription: Subscription;
  passwordSubscription: Subscription;

  constructor(
	private router: Router,
	private fb: FormBuilder,
	private userService: UsernameService,
    private userValidator: UsernameValidator
  ) { }

  ngOnInit() {
	document.body.style['background-color'] = '#f8f9fa';
	this.loginForm = this.fb.group ({
      username: ['', [ Validators.required, Validators.pattern('^[^:]*$')], this.userValidator.existingUsername() ],
      password: ['', [ Validators.required, Validators.minLength(8) ]]
	});
	this.registerForm = this.fb.group ({
      username: ['', [ Validators.required, Validators.pattern('^[^:]*$') ], this.userValidator.notExistingUsername()],
      email: ['', [ Validators.required, Validators.email ]],
      password: ['', [ Validators.required, Validators.minLength(8) ]],
      repeat_password: ['']
	}, {
	  validator: matchPasswordValidator
	});
	this.onChanges();
  }
  
  onChanges () {
    this.usernameSubscription = this.loginForm.get('username').valueChanges.subscribe(value => {
	  if (this.invalidLoginForm) {
		this.invalidLoginForm = false;
	  }
	});
    this.passwordSubscription = this.loginForm.get('password').valueChanges.subscribe(value => {
	  if (this.invalidLoginForm) {
		this.invalidLoginForm = false;
	  }
	});
  }
  
  ngOnDestroy() {
    this.usernameSubscription.unsubscribe();
    this.passwordSubscription.unsubscribe();
	document.body.style['background-color'] = 'white';
  }
  
  submitLoginForm () {
	return this.userService.checkValidLoginForm(this.loginForm.value).subscribe((access: boolean) => {
	  if (access) {
		this.router.navigate(['/ia-farming']);
	  } else {
		this.invalidLoginForm = true;
      }
	}, (error: any) => {
	  console.log(error);
	});
  }
  
  submitRegisterForm () {
	return this.userService.checkValidRegisterForm(this.registerForm.value).subscribe((access: boolean) => {
	  if (access) {
		this.router.navigate(['/ia-farming']);
	  } else {
		console.log('error: hubo un error procesando la petición.');
	  }
	}, (error: any) => {
	  console.log(error);
	});
  }

}
