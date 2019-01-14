import { Component, OnInit, OnDestroy } from '@angular/core';
import { Router } from '@angular/router';

@Component({
  selector: 'app-app00',
  templateUrl: './app00.component.html',
  styleUrls: ['./app00.component.css']
})

export class App00Component implements OnInit, OnDestroy {

  constructor(
    private router: Router
  ) { }

  ngOnInit() {
	document.documentElement.style['height'] = '100%';
	document.body.style['height'] = '100%';
	document.body.style['background-color'] = '#f8f9fa';
  }
  
  ngOnDestroy() {
	document.body.style['background-color'] = 'white';
	document.body.style['height'] = 'unset';
	document.documentElement.style['height'] = 'unset';
  }
  
  logout () {
	this.router.navigate(['/ia-farming/login'])
  }
}
