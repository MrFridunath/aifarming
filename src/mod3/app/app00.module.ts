import { NgModule } from '@angular/core';
import { RouterModule, Routes, CanActivate, CanLoad } from '@angular/router';

import { SharedModule } from '../app.shared-module';
import { ReactiveFormsModule } from '@angular/forms';

import { App00AuthGuard } from './auth-guards/app00.auth-guard';
import { UsernameService } from './services/username.service';
import { UsernameValidator } from './validators/username.validator';


import { App00LoginComponent } from './login/login.component';

const app00Routes: Routes = [
  {
    path: 'login', 
	component: App00LoginComponent 
  },
  {
    path: '',
    loadChildren: './main/app00.main.module#App00MainModule',
	canLoad: [ App00AuthGuard ],
  },
  {
    path: '**',
	redirectTo: ''
  }
];

@NgModule({
  declarations: [
    App00LoginComponent
  ],
  imports: [
	SharedModule,
    ReactiveFormsModule,
	RouterModule.forChild(
	  app00Routes
	)
  ],
  providers: [ App00AuthGuard, UsernameService, UsernameValidator ]
})
export class App00Module { }
