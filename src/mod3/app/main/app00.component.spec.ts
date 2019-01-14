import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { App00Component } from './app00.component';

describe('App00Component', () => {
  let component: App00Component;
  let fixture: ComponentFixture<App00Component>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [ App00Component ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(App00Component);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
