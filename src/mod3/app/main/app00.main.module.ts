import { NgModule } from '@angular/core';
import { RouterModule, Routes, CanActivate, CanActivateChild } from '@angular/router';

import { SharedModule } from '../../app.shared-module';
import { ReactiveFormsModule } from '@angular/forms';

import { App00AuthGuard } from '../auth-guards/app00.auth-guard';

import { App00Component } from './app00.component';

import { App00HomeComponent } from './home/app00.home.component';
import { App00SoftwareComponent } from './software/app00.software.component';
import { App00StoreComponent } from './store/app00.store.component';

const app00MainRoutes: Routes = [
  {
    path: '',
	component: App00Component,
	canActivate: [ App00AuthGuard ],
	canActivateChild: [ App00AuthGuard ],
	children: [
	  {
		path: '',
		component: App00HomeComponent
	  },
	  {
		path: 'software',
		component: App00SoftwareComponent
	  },
	  {
		path: 'store',
		component: App00StoreComponent
	  },
	]
  },
  {
    path: '**',
	redirectTo: '/ia-farming',
	pathMatch: 'full'
  }
];

@NgModule({
  declarations: [
    App00Component,
    App00HomeComponent,
    App00SoftwareComponent,
    App00StoreComponent
  ],
  imports: [
	SharedModule,
    ReactiveFormsModule,
	RouterModule.forChild(
      app00MainRoutes
    )
  ],
  providers: [ App00AuthGuard ]
})
export class App00MainModule { }
