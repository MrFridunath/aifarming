import { Component, OnInit, OnDestroy } from '@angular/core';
import { FormBuilder, FormGroup, FormControl, Validators } from '@angular/forms';
import { trigger, state, style, animate, transition } from '@angular/animations';
import { HttpClient, HttpHeaders } from '@angular/common/http';
import { timer as observableTimer, Subscription } from 'rxjs';
import { Router } from '@angular/router';

import { System } from '../../objects/system.object';
import { Harvest } from '../../objects/harvest.object';

@Component({
  selector: 'app-app00-home',
  templateUrl: './app00.home.component.html',
  styleUrls: ['./app00.home.component.css'],
  animations: [
    trigger('newSystemConfigurationImageAnimation', [
      state('normal', style({ transform: 'rotate(0deg) scale(1.0)' })),
      state('normalZoom', style({ transform: 'rotate(0deg) scale(1.3)' })),
      state('config', style({ transform: 'rotate(45deg) scale(1.0)' })),
      state('configZoom', style({ transform: 'rotate(45deg) scale(1.3)' })),
      transition('normal => normalZoom', animate('100ms')),
      transition('normalZoom => normal', animate('100ms')),
      transition('normalZoom => configZoom', animate('200ms')),
      transition('configZoom => normalZoom', animate('200ms')),
      transition('config => configZoom', animate('100ms')),
      transition('configZoom => config', animate('100ms')),
      transition('config => normal', animate('200ms')),
      transition('normal => config', animate('200ms'))
    ])
  ]
})

export class App00HomeComponent implements OnInit, OnDestroy {

  systemForm: FormGroup;
  harvestForm: FormGroup;

  mySystems: System[] = [];
  myHarvests: Harvest[] = [];

  checkMySystems: Subscription[] = [];

  showVisualizationSystemSetting: boolean[] = [];
  // showFertilizationSystemSetting: boolean[] = [];
  showModificationSystemSetting: boolean[] = [];
  editSystemForm: FormGroup[] = [];

  showNewSystemConfiguration: boolean = false;

  showNotification: boolean = false;
  notificationId: number = 0;

  newSystemConfigurationImageState: string = 'normal';

  constructor(
  	private fb: FormBuilder,
	private http: HttpClient,
	private router: Router
  ) { }

  ngOnInit() {
	this.systemForm = this.fb.group ({
      ip: [''],
      port: [''],
	  username: [''],
	  password: [''],
	  name: ['']
	});
	this.harvestForm = this.fb.group ({
      name: [''],
      ph: ['7.0'],
	  threshold: ['0.02']
	});
	
	if (localStorage.getItem('token-app00') == null) {
	  this.router.navigate(['/ia-farming/login']);
	  return;
	}
	
	let username = JSON.parse(atob(localStorage.getItem('token-app00').substring(localStorage.getItem('token-app00').indexOf('.')+1, localStorage.getItem('token-app00').lastIndexOf('.')))).username;
	
	let body = {
	  username: username
	};
    let headers = new HttpHeaders({
	  'Content-Type' : 'application/json'
	});
    return this.http.post('/app00/get_systems', JSON.stringify(body), {headers: headers, observe: 'response'}).subscribe((data: Object) => {
	  this.myHarvests = data['body']['harvests'];
	  this.mySystems = data['body']['systems'];
      for (let i in this.mySystems) {
	    let timer = observableTimer(1000 * 10);
	    let timerNotification = timer.subscribe( t => {
	      let object = {
	        username: username,
	        id: i
	      };
	      let headers = new HttpHeaders({
	        'Content-Type' : 'application/json'
	      });
  	      return this.http.post('/app00/check_system', JSON.stringify(object), {headers: headers, observe: 'response'}).subscribe((data: Object) => {
			this.mySystems[i].phM = data['body']['ph'];
			this.mySystems[i].airTemperatureM = data['body']['airTemperature'];
			this.mySystems[i].airHumidityM = data['body']['airHumidity'];
			this.mySystems[i].soilHumidityM = data['body']['soilHumidity'];
			this.mySystems[i].rainM = data['body']['rain'];
			this.mySystems[i].lightM = data['body']['light'];
			this.mySystems[i].lastWateringM = data['body']['lastWatering'];
	        this.newSystemTemporizator(i);
          });
        });

        this.checkMySystems.push(timerNotification);
		let particularEditSystemForm = this.fb.group ({
          name: this.mySystems[i].name,
          type: this.mySystems[i].harvest.name
  	    });
  	    this.editSystemForm.push(particularEditSystemForm);
		this.showVisualizationSystemSetting.push(false);
		this.showModificationSystemSetting.push(false);
      }
    }, (error: any) => {
	  console.log(error);
	  this.router.navigate(['/ia-farming/login']);
	});
  }
  
  ngOnDestroy() {
    for (let i in this.checkMySystems) {
	  this.checkMySystems[i].unsubscribe();
	}
	this.saveSystems();
	this.saveProfiles();
  }
  
  addSystem () {
	this.showNewSystemConfiguration = false;
    this.notificationId = 0;
	this.showNotification = true;

    let system = {
      id: this.mySystems.length,
      name: 'N\\A',
  	  ip: '127.0.0.1',
  	  port: 40500,
  	  username: 'admin',
	  password: 'root',
      harvest: {
		name: 'N\\A',
        ph: parseFloat(this.harvestForm.controls.ph.value),
        threshold: parseFloat(this.harvestForm.controls.threshold.value)
      },
      phM: 0,
      airTemperatureM: 0,
      airHumidityM: 0,
      soilHumidityM: 0,
      rainM: 0,
      lightM: 0,
      lastWateringM: 0
    };

	if (this.systemForm.controls.name.value !== '') {
	  system.name = this.systemForm.controls.name.value;
	}
	  
	if (this.harvestForm.controls.name.value !== '') {
	  system.harvest.name = this.harvestForm.controls.name.value;
	}

	if (this.systemForm.controls.ip.value !== '') {
	  system.ip = this.systemForm.controls.ip.value;
	}
	  
	if (this.systemForm.controls.port.value !== '') {
      try {
	    system.port = parseInt(this.systemForm.controls.port.value);
      } catch (SyntaxException) {
        system.port = 40500;
      }
	}

	if (this.systemForm.controls.username.value !== '') {
	  system.username = this.systemForm.controls.username.value;
	}
	  
	if (this.systemForm.controls.password.value !== '') {
	  system.password = this.systemForm.controls.password.value;
	}

	let headers = new HttpHeaders({
	  'Content-Type' : 'application/json'
	});

    return this.http.post('/app00/add_system', JSON.stringify(system), {headers: headers, observe: 'response'}).subscribe((data: any) => {

      this.mySystems.push(system);
      this.saveSystems();

	  let i = (this.mySystems.length - 1);
	  
      let timer = observableTimer(1000 * 10);
	  let timerNotification = timer.subscribe( t => {
		let username = JSON.parse(atob(localStorage.getItem('token-app00').substring(localStorage.getItem('token-app00').indexOf('.')+1, localStorage.getItem('token-app00').lastIndexOf('.')))).username;
        let object = {
	      username: username,
	      id: i
	    };
	    let headers = new HttpHeaders({
	      'Content-Type' : 'application/json'
	    });
  	    return this.http.post('/app00/check_system', JSON.stringify(object), {headers: headers, observe: 'response'}).subscribe((data: Object) => {
          this.mySystems[i].phM = data['body']['ph'];
          this.mySystems[i].airTemperatureM = data['body']['airTemperature'];
          this.mySystems[i].airHumidityM = data['body']['airHumidity'];
          this.mySystems[i].soilHumidityM = data['body']['soilHumidity'];
          this.mySystems[i].rainM = data['body']['rain'];
          this.mySystems[i].lightM = data['body']['light'];
          this.mySystems[i].lastWateringM = data['body']['lastWatering'];
	      this.newSystemTemporizator(i);
        }, (error: any) => {
          this.newSystemTemporizator(i);
        });
      });

      this.checkMySystems.push(timerNotification);

      let particularEditSystemForm = this.fb.group({
        name: this.mySystems[i].name,
        type: this.mySystems[i].harvest.name
      });

      this.editSystemForm.push(particularEditSystemForm);
      this.showVisualizationSystemSetting.push(false);
      this.showModificationSystemSetting.push(false);

	  this.notificationId = 200.1;

	}, (error: any) => {
      console.log(error);
	  if (error.status === 401) {
        this.notificationId = 401;
	  }
	  else if (error.status === 409) {
        this.notificationId = 409;
	  }
	  else if (error.status === 410) {
        this.notificationId = 410.1;
	  }
      else if (error.status === 500) {
        this.notificationId = 500;
      }
      else {
	 	this.notificationId = 404;
	  }
	});
  }
  
  editSystem (index) {
	this.saveSystems();
  }

  deleteSystem (index) {
	this.showNewSystemConfiguration = false;
    this.notificationId = 0;
	this.showNotification = true;

    let username = JSON.parse(atob(localStorage.getItem('token-app00').substring(localStorage.getItem('token-app00').indexOf('.')+1, localStorage.getItem('token-app00').lastIndexOf('.')))).username;

    let object = {
      username: username,
      id: index
    };

    let headers = new HttpHeaders({
      'Content-Type' : 'application/json'
    });

    return this.http.post('/app00/delete_system', JSON.stringify(object), {headers: headers, observe: 'response'}).subscribe((data: any) => {
      this.checkMySystems[index].unsubscribe();
	  this.mySystems.splice(index, 1);
      for (let i = index; i < this.mySystems.length; i++) {
        this.checkMySystems[i].unsubscribe();
        this.mySystems[i].id = i;
	    this.newSystemTemporizator(i);
      }
      this.checkMySystems.splice(index, 1);
      this.editSystemForm.splice(index, 1);
      this.showModificationSystemSetting.splice(index, 1);
      this.showVisualizationSystemSetting.splice(index, 1);
      this.saveSystems();

      this.notificationId = 200.2;
	  
	}, (error: any) => {
      console.log(error);
	  if (error.status === 401) {
        this.notificationId = 401;
	  }
	  else if (error.status === 408) {
        this.notificationId = 408;
	  }
	  else if (error.status === 409) {
        this.notificationId = 409;
	  }
	  else if (error.status === 410) {
        this.notificationId = 410.2;
	  }
      else if (error.status === 500) {
        this.notificationId = 500;
      }
      else {
	 	this.notificationId = 404;
	  }
	});
  }

  sendAction (index, code) {
    this.showNewSystemConfiguration = false;
    this.notificationId = 0;
	this.showNotification = true;

    let username = JSON.parse(atob(localStorage.getItem('token-app00').substring(localStorage.getItem('token-app00').indexOf('.')+1, localStorage.getItem('token-app00').lastIndexOf('.')))).username;

    let object = {
      username: username,
      id: index,
      code: code
    };

    let headers = new HttpHeaders({
      'Content-Type' : 'application/json'
    });

    return this.http.post('/app00/send_action', JSON.stringify(object), {headers: headers, observe: 'response'}).subscribe((data: any) => {
      this.notificationId = 200.3;
	}, (error: any) => {
      console.log(error);
	  if (error.status === 401) {
        this.notificationId = 401;
	  }
	  else if (error.status === 408) {
        this.notificationId = 408;
	  }
	  else if (error.status === 409) {
        this.notificationId = 409;
	  }
	  else if (error.status === 410) {
        this.notificationId = 410.2;
	  }
      else if (error.status === 500) {
        this.notificationId = 500;
      }
      else {
	 	this.notificationId = 404;
	  }
	});
  }

  saveSystems () {
    let headers = new HttpHeaders({
	  'Content-Type': 'application/json'
	});
	let username = JSON.parse(atob(localStorage.getItem('token-app00').substring(localStorage.getItem('token-app00').indexOf('.')+1, localStorage.getItem('token-app00').lastIndexOf('.')))).username;
	let body = {
	  username: username,
	  systems: this.mySystems
	}
    return this.http.post('/app00/set_systems', JSON.stringify(body), { headers: headers }).subscribe((data: any) => {}, (error: any) => {
	  console.log(error);
	});
  }
  
  saveProfiles () {
  }

  newSystemTemporizator (id) {
	let username = JSON.parse(atob(localStorage.getItem('token-app00').substring(localStorage.getItem('token-app00').indexOf('.')+1, localStorage.getItem('token-app00').lastIndexOf('.')))).username;
	let object = {
	  username: username,
  	  id: id
    };

	let headers = new HttpHeaders({
      'Content-Type' : 'application/json'
	});

	let timer = observableTimer(10000);
	let timerNotification = timer.subscribe( t => {
  	  return this.http.post('/app00/check_system', JSON.stringify(object), {headers: headers, observe: 'response'}).subscribe((data: Object) => {
        this.mySystems[id].phM = data['body']['ph'];
        this.mySystems[id].airTemperatureM = data['body']['airTemperature'];
        this.mySystems[id].airHumidityM = data['body']['airHumidity'];
        this.mySystems[id].soilHumidityM = data['body']['soilHumidity'];
        this.mySystems[id].rainM = data['body']['rain'];
        this.mySystems[id].lightM = data['body']['light'];
        this.mySystems[id].lastWateringM = data['body']['lastWatering'];
	    this.newSystemTemporizator(id);
      }, (error: any) => {
        this.newSystemTemporizator(id);
      });
    });
    this.checkMySystems[id] = timerNotification;
  }
  
  toggleConfiguration() {
    if(this.newSystemConfigurationImageState === 'normalZoom'){
	  this.newSystemConfigurationImageState = 'configZoom';
	}
	else if (this.newSystemConfigurationImageState === 'configZoom'){
	  this.newSystemConfigurationImageState = 'normalZoom';
	}
	else if (this.newSystemConfigurationImageState === 'config'){
	  this.newSystemConfigurationImageState = 'normal';
	}
	else {
	  this.newSystemConfigurationImageState = 'config';
	}
	this.showNewSystemConfiguration = !this.showNewSystemConfiguration;
  }
  
  configZoom() {
	if(this.newSystemConfigurationImageState === 'normalZoom') {
	  this.newSystemConfigurationImageState = 'normal';
	}
	else if(this.newSystemConfigurationImageState === 'normal') {
	  this.newSystemConfigurationImageState = 'normalZoom';
	}
	else if(this.newSystemConfigurationImageState === 'config') {
	  this.newSystemConfigurationImageState = 'configZoom';
	}
	else {
	  this.newSystemConfigurationImageState = 'config';
	}
  }
}
